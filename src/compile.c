#include "defines.h"
#include "array.h"

#include <string.h>

#define WSC_READ_BUFFER_SIZE 256
#define WSC_INIT_INSTR_CAP 16
#define WSC_INIT_LABEL_CAP 16

typedef struct {
    char window[WSC_READ_BUFFER_SIZE];
    size_t index, count;
    ws_rdfn_t fn; void* ud;
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
        rdbuf->count = rdbuf->fn(
            rdbuf->window, 1, WSC_READ_BUFFER_SIZE, rdbuf->ud);
        if (rdbuf->count) goto retry;
        return WSA_EOF;
    }
}

typedef WSM_ARRAY_STRUCT(wsi_instr_t, instrs) wsi_instrs_t;

WSM_ARRAY_RESERVE(wsi_instr, wsi_instrs_t, instrs)
WSM_ARRAY_PUSH   (wsi_instr, wsi_instrs_t, instrs, wsi_instr_t)

static ws_error_t wsi_instr_push_data(wsi_instrs_t* array, void* data, size_t size) {
    if (wsi_instr_reserve(array, size)) return WSE_NO_MEMORY;
    memcpy(array->instrs + array->count, data, size); array->count += size;
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

static ws_error_t wsi_parse_stk_manip(wsi_instrs_t* a, wsi_read_buffer_t* rb) {
    ws_int_t arg; ws_error_t ec;

    switch (wsi_rb_get(rb)) {
        case WSA_SPACE:
            if ((ec = wsi_parse_integer(rb, &arg))) return ec;
            if ((ec = wsi_instr_push(a, WSI_PUSH))) return ec;
            if ((ec = wsi_instr_push_data(a, &arg, sizeof arg))) return ec;
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
                    if (arg < 0) return WSE_INVAL_PARAM;
                    if ((ec = wsi_instr_push(a, WSI_COPY))) return ec;
                    if ((ec = wsi_instr_push_data(a, &arg, sizeof arg))) return ec;
                    break;

                case WSA_LF:
                    if ((ec = wsi_parse_integer(rb, &arg))) return ec;
                    if (arg < 0) return WSE_INVAL_PARAM;
                    if (arg == 0) break;
                    if ((ec = wsi_instr_push(a, WSI_SLIDE))) return ec;
                    if ((ec = wsi_instr_push_data(a, &arg, sizeof arg))) return ec;
                    break;

                case WSA_TAB: return WSE_INVAL_INSTR;
                case WSA_EOF: return WSE_INCOMPL_INSTR;
            } break;

        case WSA_EOF: return WSE_INCOMPL_INSTR;
    }

    return WSE_OK;
}

static ws_error_t wsi_parse_arith(wsi_instrs_t* a, wsi_read_buffer_t* rb) {
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

static ws_error_t wsi_parse_heap_acs(wsi_instrs_t* a, wsi_read_buffer_t* rb) {
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

static ws_error_t wsi_parse_io(wsi_instrs_t* a, wsi_read_buffer_t* rb) {
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

static ws_error_t wsi_parse_label(wsi_read_buffer_t* rb, wsi_label_t* lbl) {
    wsa_char_t ch = wsi_rb_get(rb); size_t i = 0;
    /**/ if (ch == WSA_LF ) return WSE_INVAL_LABEL;
    else if (ch == WSA_EOF) return WSE_INCOMPL_LABEL;

    memset(lbl->parts, 0, WSL_PARTS_BYTE);

    do {
        wsl_part_t bit = ch == WSA_TAB;
        lbl->parts[i / WSL_PART_BITS] |= bit << (i % WSL_PART_BITS);
        ch = wsi_rb_get(rb);
        if (WSA_EOF) return WSE_INCOMPL_LABEL;
    } while (++i < WSL_BITS && ch != WSA_LF);
    if (ch == WSA_EOF) return WSE_INCOMPL_LABEL;
    if (ch != WSA_LF ) return WSE_TOO_LONG_LABEL;

    lbl->place = WSL_INVAL_PLACE;
    lbl->length = i;

    return WSE_OK;
}

static ws_error_t wsi_parse_flow_ctrl(
    wsi_instrs_t* a, wsl_list_t* ll, wsi_read_buffer_t* rb
) {
    ws_error_t ec; wsi_label_t label; wsl_index_t index;

    switch (wsi_rb_get(rb)) {
        case WSA_SPACE:
            switch (wsi_rb_get(rb)) {
                case WSA_SPACE: {
                    if ((ec = wsi_parse_label(rb, &label))) return ec;
                    if ((ec = wsl_getx(ll, &label, &index))) return ec;
                    if ((ec = wsi_instr_push(a, WSI_MARK))) return ec;
                    ll->labels[index].place = a->count - 1;
                    if ((ec = wsi_instr_push_data(a, &index, sizeof index))) return ec;
                } break;

                case WSA_TAB: {
                    if ((ec = wsi_parse_label(rb, &label))) return ec;
                    if ((ec = wsl_get(ll, &label, &index))) return ec;
                    if ((ec = wsi_instr_push(a, WSI_CALL))) return ec;
                    if ((ec = wsi_instr_push_data(a, &index, sizeof index))) return ec;
                } break;

                case WSA_LF: {
                    if ((ec = wsi_parse_label(rb, &label))) return ec;
                    if ((ec = wsl_get(ll, &label, &index))) return ec;
                    if ((ec = wsi_instr_push(a, WSI_GOTO))) return ec;
                    if ((ec = wsi_instr_push_data(a, &index, sizeof index))) return ec;
                } break;

                case WSA_EOF: return WSE_INCOMPL_INSTR;
            } break;

        case WSA_TAB:
            switch (wsi_rb_get(rb)) {
                case WSA_SPACE: {
                    if ((ec = wsi_parse_label(rb, &label))) return ec;
                    if ((ec = wsl_get(ll, &label, &index))) return ec;
                    if ((ec = wsi_instr_push(a, WSI_IFZR))) return ec;
                    if ((ec = wsi_instr_push_data(a, &index, sizeof index))) return ec;
                } break;

                case WSA_TAB: {
                    if ((ec = wsi_parse_label(rb, &label))) return ec;
                    if ((ec = wsl_get(ll, &label, &index))) return ec;
                    if ((ec = wsi_instr_push(a, WSI_IFNG))) return ec;
                    if ((ec = wsi_instr_push_data(a, &index, sizeof index))) return ec;
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
    ws_code_t** cptr, void* src, ws_rdfn_t rdr,
    ws_alloc_t alloc, void* udata
) {
    wsi_read_buffer_t rdbuf[1] = {0};
    wsi_instrs_t arr[1] = {0};
    wsl_list_t   lst[1] = {0};

    ws_code_t* code;
    ws_error_t ec;
    size_t i;

    if (!cptr || !rdr || !alloc) return WSE_INVAL_ARG;

    *cptr = NULL; code = alloc(NULL, sizeof *code, udata);
    if (!code) return WSE_NO_MEMORY;

    memset(code, 0, sizeof *code);
    code->alloc = arr->fn = lst->fn = alloc;
    code->udata = arr->ud = lst->ud = udata;
    rdbuf->fn = rdr;
    rdbuf->ud = src;

    WSM_ARRAY_INIT(arr, instrs, WSC_INIT_INSTR_CAP, WSM_THROW(WSE_NO_MEMORY));
    WSM_ARRAY_INIT(lst, labels, WSC_INIT_LABEL_CAP, WSM_THROW(WSE_NO_MEMORY));

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
                i += sizeof(wsl_index_t);
        } else {
            wsl_index_t index;
            memcpy(&index, arr->instrs + i + 1, sizeof index);
            if (index >= lst->count) WSM_THROW(WSE_UNKNOWN_LABEL);
            if (lst->labels[index].place == WSL_INVAL_PLACE)
                WSM_THROW(WSE_NODEF_LABEL);
            i += sizeof(wsl_index_t);
        }
    }

    WSM_ARRAY_SHRINK(arr, instrs, WSM_THROW(WSE_NO_SHRINK));
    WSM_ARRAY_SHRINK(lst, labels, WSM_THROW(WSE_NO_SHRINK));

    code->instrs = arr->instrs; code->icnt = arr->count;
    code->labels = lst->labels; code->lcnt = lst->count;

    *cptr = code;
    return WSE_OK;
error:
    lst->fn(lst->labels, 0, lst->ud);
    arr->fn(arr->instrs, 0, arr->ud);
    alloc(code, 0, udata);
    return ec;
}