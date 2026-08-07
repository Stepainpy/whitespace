#include "defines.h"

#include <string.h>

#define WSC_READ_BUFFER_SIZE 256
#define WSC_INIT_INSTR_CAP 16

typedef enum {
    WSA_EOF = 0,
    WSA_SPACE,
    WSA_TAB,
    WSA_LF
} wsa_char_t;

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

#define WSM_THROW(err_code) \
    do { ec = err_code; goto error; } while (0)

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

static ws_error_t wsi_parse_arith    (wsi_array_t* a, wsi_read_buffer_t* rb) { return WSE_NOT_IMPL; (void)a, (void)rb; }
static ws_error_t wsi_parse_heap_acs (wsi_array_t* a, wsi_read_buffer_t* rb) { return WSE_NOT_IMPL; (void)a, (void)rb; }
static ws_error_t wsi_parse_io       (wsi_array_t* a, wsi_read_buffer_t* rb) { return WSE_NOT_IMPL; (void)a, (void)rb; }
static ws_error_t wsi_parse_flow_ctrl(wsi_array_t* a, wsi_read_buffer_t* rb) { return WSE_NOT_IMPL; (void)a, (void)rb; }

ws_error_t ws_compile(
    ws_code_t** cptr,
    void* src, ws_read_t rdr,
    ws_alloc_t alloc, void* udata
) {
    wsi_read_buffer_t rdbuf[1] = {0};
    wsi_array_t arr[1] = {0};
    ws_code_t* code;
    ws_error_t ec;

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
                ec = wsi_parse_flow_ctrl(arr, rdbuf);
                if (ec) goto error;
                break;

            case WSA_EOF: goto loop_exit;
        }
    }
loop_exit:

    code->instrs = arr->instrs;
    code->count  = arr->count ;

    *cptr = code;
    return WSE_OK;
error:
    alloc(arr->instrs, 0, udata);
    alloc(code, 0, udata);
    return ec;
}