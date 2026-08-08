#include "defines.h"

#include <string.h>

#define WSC_READ_BUFFER_SIZE 256
#define WSC_INIT_INSTR_CAP 16
#define WSC_INIT_LABEL_CAP 16

typedef struct {
    char window[WSC_READ_BUFFER_SIZE];
    size_t index, count;
    ws_read_t func; void* ud;
} wsi_read_buffer_t;

static wsa_char_t wsi_rb_get(wsi_read_buffer_t* rdbuf) {
retry:
    if (rdbuf->index < rdbuf->count) {
        switch (rdbuf->window[rdbuf->index++]) {
            case WSC_S_CHAR: return WSA_SPACE;
            case WSC_T_CHAR: return WSA_TAB;
            case WSC_L_CHAR: return WSA_LF;
            default: goto retry;
        }
    } else {
        rdbuf->index = 0;
        rdbuf->count = rdbuf->func(
            rdbuf->window, 1, WSC_READ_BUFFER_SIZE, rdbuf->ud);
        if (rdbuf->count) goto retry;
        return WSA_EOF;
    }
}

typedef struct {
    wsi_instr_t* instrs;
    size_t count, capacity;
    void* ud; ws_alloc_t fn;
} wsi_array_t;

static ws_error_t wsi_instr_reserve(wsi_array_t* a, size_t need) {
    size_t newcap; void* newptr;
    if (a->count + need <= a->capacity) return WSE_OK;

    newcap = a->capacity;
    while (a->count + need > newcap)
        newcap = (newcap * 207 + 127) / 128;

    newptr = a->fn(a->instrs, newcap, a->ud);
    if (!newptr) return WSE_NO_MEMORY;

    a->instrs   = newptr;
    a->capacity = newcap;
    return WSE_OK;
}

static ws_error_t wsi_instr_push(wsi_array_t* a, wse_instr_t instr) {
    if (wsi_instr_reserve(a, 1)) return WSE_NO_MEMORY;
    a->instrs[a->count++] = instr;
    return WSE_OK;
}

static ws_error_t wsi_instr_push_int(wsi_array_t* a, ws_int_t integer) {
    size_t i; if (wsi_instr_reserve(a, sizeof integer))
        return WSE_NO_MEMORY;
    for (i = 0; i < sizeof integer; i++, integer >>= 8)
        a->instrs[a->count++] = integer & 0xFF;
    return WSE_OK;
}

static ws_error_t wsi_parse_integer(wsi_read_buffer_t* rb, ws_int_t* out) {
    wsa_char_t ch; int neg;

    ch = wsi_rb_get(rb);
    /**/ if (ch == WSA_SPACE) neg = 0;
    else if (ch == WSA_TAB  ) neg = 1;
    else return WSE_INVAL_SIGN;

    *out = 0;
    while ((ch = wsi_rb_get(rb)) != WSA_LF) {
        if (ch == WSA_EOF) return WSE_INCOMPL_INT;

        *out = *out << 1 | (ch == WSA_TAB);
        if (*out < 0) return WSE_INT_OVERFLOW;
    }
    if (neg) *out = -(*out);

    return WSE_OK;
}

static ws_error_t wsi_parse_stk_manip(wsi_array_t* a, wsi_read_buffer_t* rb) {
    ws_int_t arg; ws_error_t ec;

    switch (wsi_rb_get(rb)) {
        case WSA_SPACE:
            if ((ec = wsi_parse_integer(rb, &arg))) return ec;

            if ((ec = wsi_instr_push    (a, WSI_PUSH))) return ec;
            if ((ec = wsi_instr_push_int(a,      arg))) return ec;

            break;

        case WSA_LF:
            switch (wsi_rb_get(rb)) {
                case WSA_SPACE:
                    if ((ec = wsi_instr_push(a, WSI_DUP))) return ec;
                    break;
                case WSA_TAB:
                    if ((ec = wsi_instr_push(a, WSI_SWAP))) return ec;
                    break;
                case WSA_LF:
                    if ((ec = wsi_instr_push(a, WSI_DROP))) return ec;
                    break;

                case WSA_EOF: return WSE_INCOMPL_INSTR;
            } break;

        case WSA_TAB:
            switch (wsi_rb_get(rb)) {
                case WSA_SPACE:
                    if ((ec = wsi_parse_integer(rb, &arg))) return ec;

                    if ((ec = wsi_instr_push    (a, WSI_COPY))) return ec;
                    if ((ec = wsi_instr_push_int(a,      arg))) return ec;

                    break;
                case WSA_LF:
                    if ((ec = wsi_parse_integer(rb, &arg))) return ec;

                    if ((ec = wsi_instr_push    (a, WSI_SLIDE))) return ec;
                    if ((ec = wsi_instr_push_int(a,       arg))) return ec;

                    break;

                case WSA_TAB: return WSE_INVAL_INSTR;
                case WSA_EOF: return WSE_INCOMPL_INSTR;
            } break;

        case WSA_EOF: return WSE_INCOMPL_INSTR;
    }

    return WSE_OK;
}

static ws_error_t wsi_parse_arith(wsi_array_t* a, wsi_read_buffer_t* rb) {
    ws_error_t ec;

    switch (wsi_rb_get(rb)) {
        case WSA_SPACE:
            switch (wsi_rb_get(rb)) {
                case WSA_SPACE:
                    if ((ec = wsi_instr_push(a, WSI_ADD))) return ec;
                    break;
                case WSA_TAB:
                    if ((ec = wsi_instr_push(a, WSI_SUB))) return ec;
                    break;
                case WSA_LF:
                    if ((ec = wsi_instr_push(a, WSI_MUL))) return ec;
                    break;
                case WSA_EOF: return WSE_INCOMPL_INSTR;
            } break;

        case WSA_TAB:
            switch (wsi_rb_get(rb)) {
                case WSA_SPACE:
                    if ((ec = wsi_instr_push(a, WSI_DIV))) return ec;
                    break;
                case WSA_TAB:
                    if ((ec = wsi_instr_push(a, WSI_MOD))) return ec;
                    break;
                case WSA_LF: return WSE_INVAL_INSTR;
                case WSA_EOF: return WSE_INCOMPL_INSTR;
            } break;

        case WSA_LF: return WSE_INVAL_INSTR;
        case WSA_EOF: return WSE_INCOMPL_INSTR;
    }

    return WSE_OK;
}

static ws_error_t wsi_parse_heap_acs(wsi_array_t* a, wsi_read_buffer_t* rb) {
    ws_error_t ec;

    switch (wsi_rb_get(rb)) {
        case WSA_SPACE:
            if ((ec = wsi_instr_push(a, WSI_STORE))) return ec;
            break;
        case WSA_TAB:
            if ((ec = wsi_instr_push(a, WSI_LOAD))) return ec;
            break;

        case WSA_LF: return WSE_INVAL_INSTR;
        case WSA_EOF: return WSE_INCOMPL_INSTR;
    }

    return WSE_OK;
}

static ws_error_t wsi_parse_io(wsi_array_t* a, wsi_read_buffer_t* rb) {
    ws_error_t ec;

    switch (wsi_rb_get(rb)) {
        case WSA_SPACE:
            switch (wsi_rb_get(rb)) {
                case WSA_SPACE:
                    if ((ec = wsi_instr_push(a, WSI_OUT_CHAR))) return ec;
                    break;
                case WSA_TAB:
                    if ((ec = wsi_instr_push(a, WSI_OUT_INT))) return ec;
                    break;

                case WSA_LF: return WSE_INVAL_INSTR;
                case WSA_EOF: return WSE_INCOMPL_INSTR;
            } break;

        case WSA_TAB:
            switch (wsi_rb_get(rb)) {
                case WSA_SPACE:
                    if ((ec = wsi_instr_push(a, WSI_IN_CHAR))) return ec;
                    break;
                case WSA_TAB:
                    if ((ec = wsi_instr_push(a, WSI_IN_INT))) return ec;
                    break;

                case WSA_LF: return WSE_INVAL_INSTR;
                case WSA_EOF: return WSE_INCOMPL_INSTR;
            } break;

        case WSA_LF: return WSE_INVAL_INSTR;
        case WSA_EOF: return WSE_INCOMPL_INSTR;
    }

    return WSE_OK;
}

typedef unsigned long wsi_lbl_int_t;

#define WSC_LABEL_INT_BIT (sizeof(wsi_lbl_int_t) * 8)
#define WSC_LABEL_PARTS ((WSC_MAX_LABEL_SIZE * 2 + WSC_LABEL_INT_BIT - 1) / WSC_LABEL_INT_BIT)

#define WSC_INVAL_PLACE (~(size_t)0)

typedef struct {
    size_t place;
    wsi_lbl_int_t parts[WSC_LABEL_PARTS];
} wsi_label_t;

typedef struct {
    wsi_label_t* labels;
    size_t count, capacity;
    ws_alloc_t fn; void* ud;
} wsi_label_list_t;

static int wsi_label_equal(const wsi_label_t* lhs, const wsi_label_t* rhs) {
    size_t i; for (i = 0; i < WSC_LABEL_PARTS; i++)
        if (lhs->parts[i] != rhs->parts[i]) return 0;
    return 1;
}

static int wsi_label_find(const wsi_label_list_t* ll, const wsi_label_t* l) {
    size_t i; for (i = 0; i < ll->count; i++)
        if (wsi_label_equal(ll->labels + i, l)) return i;
    return -1;
}

static ws_error_t wsi_label_reserve(wsi_label_list_t* ll, size_t need) {
    size_t newcap; void* newptr;
    if (ll->count + need <= ll->capacity) return WSE_OK;

    newcap = ll->capacity;
    while (ll->count + need > newcap)
        newcap = (newcap * 207 + 127) / 128;

    newptr = ll->fn(ll->labels, sizeof *ll->labels * newcap, ll->ud);
    if (!newptr) return WSE_NO_MEMORY;

    ll->labels   = newptr;
    ll->capacity = newcap;
    return WSE_OK;
}

static ws_error_t wsi_label_push(wsi_label_list_t* ll, wsi_label_t* label) {
    if (wsi_label_reserve(ll, 1)) return WSE_NO_MEMORY;
    memcpy(ll->labels + ll->count++, label, sizeof *label);
    return WSE_OK;
}

static ws_error_t wsi_instr_push_label(wsi_array_t* a, wsi_label_t* label) {
    size_t i; if (wsi_instr_reserve(a, sizeof *label->parts))
        return WSE_NO_MEMORY;
    for (i = 0; i < sizeof(wsi_lbl_int_t) * WSC_LABEL_PARTS; i++)
        a->instrs[a->count++] = (
            label->parts[i / sizeof(wsi_lbl_int_t)] >> ((i % sizeof(wsi_lbl_int_t)) * 8)
        ) & 0xFF;
    return WSE_OK;
}

static ws_error_t wsi_instr_push_address(wsi_array_t* a, size_t address) {
    size_t i; if (wsi_instr_reserve(a, sizeof address))
        return WSE_NO_MEMORY;
    for (i = 0; i < sizeof address; i++, address >>= 8)
        a->instrs[a->count++] = address & 0xFF;
    return WSE_OK;
}

static size_t wsi_read_address(const wsi_instr_t* src) {
    size_t out = 0, i;
    for (i = 0; i < sizeof out; i++)
        out |= (size_t)src[i] << (i * 8);
    return out;
}

static void wsi_write_address(wsi_instr_t* src, size_t address) {
    size_t i; for (i = 0; i < sizeof address; i++, address >>= 8)
        src[i] = address & 0xFF;
}

static ws_error_t wsi_parse_label(wsi_read_buffer_t* rb, wsi_label_t* lbl) {
    wsa_char_t ch = wsi_rb_get(rb); size_t i = 0;
    /**/ if (ch == WSA_LF ) return WSE_INVAL_LABEL;
    else if (ch == WSA_EOF) return WSE_INCOMPL_LABEL;

    memset(lbl->parts, 0, sizeof *lbl->parts * WSC_LABEL_PARTS);
    lbl->place = WSC_INVAL_PLACE;

    while (i < WSC_MAX_LABEL_SIZE) {
        lbl->parts[2*i / WSC_LABEL_INT_BIT] |= (wsi_lbl_int_t)ch << (2*i % WSC_LABEL_INT_BIT);
        ch = wsi_rb_get(rb); i += 1;
        /**/ if (ch == WSA_EOF) return WSE_INCOMPL_LABEL;
        else if (ch == WSA_LF ) break;
    }
    if (i < WSC_MAX_LABEL_SIZE)
        lbl->parts[2*i / WSC_LABEL_INT_BIT] |= ch << (2*i % WSC_LABEL_INT_BIT);

    return WSE_OK;
}

static ws_error_t wsi_parse_flow_ctrl(
    wsi_array_t* a, wsi_label_list_t* ll, wsi_read_buffer_t* rb
) {
    ws_error_t ec; wsi_label_t label; int index;

    switch (wsi_rb_get(rb)) {
        case WSA_SPACE:
            switch (wsi_rb_get(rb)) {
                case WSA_SPACE: {
                    if ((ec = wsi_parse_label(rb, &label))) return ec;
                    index = wsi_label_find(ll, &label);
                    if (index >= 0 && ll->labels[index].place != WSC_INVAL_PLACE)
                        return WSE_REDEF_LABEL;
                    if (index < 0) {
                        if ((ec = wsi_label_push(ll, &label))) return ec;
                        index = ll->count - 1;
                    }

                    if ((ec = wsi_instr_push(a, WSI_MARK))) return ec;
                    ll->labels[index].place = a->count - 1;
                    if ((ec = wsi_instr_push_label(a, &label))) return ec;
                } break;

                case WSA_TAB: {
                    if ((ec = wsi_parse_label(rb, &label))) return ec;
                    index = wsi_label_find(ll, &label);
                    if (index < 0) {
                        if ((ec = wsi_label_push(ll, &label))) return ec;
                        index = ll->count - 1;
                        ll->labels[index].place = WSC_INVAL_PLACE;
                    }

                    if ((ec = wsi_instr_push(a, WSI_CALL))) return ec;
                    if ((ec = wsi_instr_push_address(a, index))) return ec;
                } break;

                case WSA_LF: {
                    if ((ec = wsi_parse_label(rb, &label))) return ec;
                    index = wsi_label_find(ll, &label);
                    if (index < 0) {
                        if ((ec = wsi_label_push(ll, &label))) return ec;
                        index = ll->count - 1;
                        ll->labels[index].place = WSC_INVAL_PLACE;
                    }

                    if ((ec = wsi_instr_push(a, WSI_GOTO))) return ec;
                    if ((ec = wsi_instr_push_address(a, index))) return ec;
                } break;

                case WSA_EOF: return WSE_INCOMPL_INSTR;
            } break;

        case WSA_TAB:
            switch (wsi_rb_get(rb)) {
                case WSA_SPACE: {
                    if ((ec = wsi_parse_label(rb, &label))) return ec;
                    index = wsi_label_find(ll, &label);
                    if (index < 0) {
                        if ((ec = wsi_label_push(ll, &label))) return ec;
                        index = ll->count - 1;
                        ll->labels[index].place = WSC_INVAL_PLACE;
                    }

                    if ((ec = wsi_instr_push(a, WSI_IFZR))) return ec;
                    if ((ec = wsi_instr_push_address(a, index))) return ec;
                } break;

                case WSA_TAB: {
                    if ((ec = wsi_parse_label(rb, &label))) return ec;
                    index = wsi_label_find(ll, &label);
                    if (index < 0) {
                        if ((ec = wsi_label_push(ll, &label))) return ec;
                        index = ll->count - 1;
                        ll->labels[index].place = WSC_INVAL_PLACE;
                    }

                    if ((ec = wsi_instr_push(a, WSI_IFNG))) return ec;
                    if ((ec = wsi_instr_push_address(a, index))) return ec;
                } break;

                case WSA_LF:
                    if ((ec = wsi_instr_push(a, WSI_RET))) return ec;
                    break;

                case WSA_EOF: return WSE_INCOMPL_INSTR;
            } break;

        case WSA_LF:
            switch (wsi_rb_get(rb)) {
                case WSA_LF:
                    if ((ec = wsi_instr_push(a, WSI_EXIT))) return ec;
                    break;

                case WSA_SPACE: case WSA_TAB: return WSE_INVAL_INSTR;
                case WSA_EOF: return WSE_INCOMPL_INSTR;
            } break;

        case WSA_EOF: return WSE_INCOMPL_INSTR;
    }

    return WSE_OK;
}

#define WSM_THROW(err_code) \
    do { ec = err_code; goto error; } while (0)

ws_error_t ws_compile(
    ws_code_t** cptr,
    void* src, ws_read_t rdr,
    ws_alloc_t alloc, void* udata
) {
    wsi_read_buffer_t rdbuf[1] = {0};
    wsi_label_list_t lst[1] = {0};
    wsi_array_t arr[1] = {0};

    ws_code_t* code;
    ws_error_t ec;
    size_t i;

    if (!cptr || !rdr || !alloc) return WSE_INVAL_ARG;
    *cptr = NULL;
    rdbuf->func = rdr;
    rdbuf->ud   = src;

    code = alloc(NULL, sizeof *code, udata);
    if (!code) return WSE_NO_MEMORY;
    memset(code, 0, sizeof *code);
    code->alloc = arr->fn = alloc;
    code->udata = arr->ud = udata;

    arr->instrs = alloc(NULL,
        (arr->capacity = WSC_INIT_INSTR_CAP), udata);
    if (!arr->instrs) WSM_THROW(WSE_NO_MEMORY);

    lst->labels = alloc(NULL,
        (lst->capacity = WSC_INIT_LABEL_CAP) * sizeof *lst->labels, udata);
    if (!lst->labels) WSM_THROW(WSE_NO_MEMORY);

    while (1) {
        switch (wsi_rb_get(rdbuf)) {
            case WSA_SPACE:
                ec = wsi_parse_stk_manip(arr, rdbuf);
                if (ec) goto error;
                break;

            case WSA_TAB:
                switch (wsi_rb_get(rdbuf)) {
                    case WSA_SPACE:
                        ec = wsi_parse_arith(arr, rdbuf);
                        if (ec) goto error;
                        break;

                    case WSA_TAB:
                        ec = wsi_parse_heap_acs(arr, rdbuf);
                        if (ec) goto error;
                        break;

                    case WSA_LF:
                        ec = wsi_parse_io(arr, rdbuf);
                        if (ec) goto error;
                        break;

                    case WSA_EOF: WSM_THROW(WSE_INCOMPL_INSTR);
                } break;

            case WSA_LF:
                ec = wsi_parse_flow_ctrl(arr, lst, rdbuf);
                if (ec) goto error;
                break;

            case WSA_EOF: goto loop_exit;
        }
    }

loop_exit:
    for (i = 0; i < arr->count; i++) {
        wsi_instr_t instr = arr->instrs[i];

        if (instr < WSI_CALL || WSI_IFNG < instr) {
            if (instr == WSI_PUSH || instr == WSI_COPY || instr == WSI_SLIDE)
                i += sizeof(ws_int_t);
            if (instr == WSI_MARK)
                i += sizeof(wsi_lbl_int_t) * WSC_LABEL_PARTS;
        } else {
            size_t index = wsi_read_address(arr->instrs + i + 1);
            if (index >= lst->count) WSM_THROW(WSE_UNKNOWN_LABEL);
            if (lst->labels[index].place == WSC_INVAL_PLACE) WSM_THROW(WSE_NODEF_LABEL);

            wsi_write_address(arr->instrs + i + 1,
                lst->labels[index].place + sizeof(wsi_lbl_int_t) * WSC_LABEL_PARTS);
            i += sizeof(wsi_lbl_int_t);
        }
    }

    code->instrs = arr->instrs;
    code->count  = arr->count ;

    *cptr = code;
    return WSE_OK;
error:
    alloc(lst->labels, 0, udata);
    alloc(arr->instrs, 0, udata);
    alloc(code, 0, udata);
    return ec;
}