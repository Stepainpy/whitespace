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

static ws_error_t wsi_instr_reserve(ws_state_t* s, size_t need) {
    size_t newcap; void* newptr;
    if (s->ip + need <= s->count) return WSE_OK;

    newcap = s->count;
    while (s->ip + need > newcap)
        newcap = (newcap * 207 + 127) / 128;

    newptr = s->alloc(s->instr, newcap, s->udata);
    if (!newptr) return WSE_NO_MEMORY;

    s->instr = newptr;
    s->count = newcap;
    return WSE_OK;
}

static ws_error_t wsi_instr_push(ws_state_t* s, wse_instr_t instr) {
    if (wsi_instr_reverse(s, 1)) return WSE_NO_MEMORY;
    s->instr[s->ip++] = instr;
    return WSE_OK;
}

static ws_error_t wsi_instr_push_int(ws_state_t* s, ws_int_t integer) {
    size_t i; if (wsi_instr_reverse(s, sizeof integer))
        return WSE_NO_MEMORY;
    for (i = 0; i < sizeof integer; i++, integer >>= 8)
        s->instr[s->ip++] = integer & 0xFF;
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

static ws_error_t wsi_parse_stk_manip(ws_state_t* s, wsi_read_buffer_t* rb) {
    ws_int_t arg; ws_error_t ec;

    switch (wsi_rb_get(rb)) {
        case WSA_SPACE:
            if ((ec = wsi_parse_integer(rb, &arg))) return ec;

            if ((ec = wsi_instr_push    (s, WSI_PUSH))) return ec;
            if ((ec = wsi_instr_push_int(s,      arg))) return ec;

            break;

        case WSA_LF:
            switch (wsi_rb_get(rb)) {
                case WSA_SPACE:
                    if ((ec = wsi_instr_push(s, WSI_DUP))) return ec;
                    break;
                case WSA_TAB:
                    if ((ec = wsi_instr_push(s, WSI_SWAP))) return ec;
                    break;
                case WSA_LF:
                    if ((ec = wsi_instr_push(s, WSI_DROP))) return ec;
                    break;

                case WSA_EOF: return WSE_INCOMPL_INSTR;
            } break;

        case WSA_TAB:
            switch (wsi_rb_get(rb)) {
                case WSA_SPACE:
                    if ((ec = wsi_parse_integer(rb, &arg))) return ec;

                    if ((ec = wsi_instr_push    (s, WSI_COPY))) return ec;
                    if ((ec = wsi_instr_push_int(s,      arg))) return ec;

                    break;
                case WSA_LF:
                    if ((ec = wsi_parse_integer(rb, &arg))) return ec;

                    if ((ec = wsi_instr_push    (s, WSI_SLIDE))) return ec;
                    if ((ec = wsi_instr_push_int(s,       arg))) return ec;

                    break;

                case WSA_TAB: return WSE_INVAL_INSTR;
                case WSA_EOF: return WSE_INCOMPL_INSTR;
            } break;

        case WSA_EOF: return WSE_INCOMPL_INSTR;
    }

    return WSE_OK;
}

static ws_error_t wsi_parse_arith(ws_state_t* s, wsi_read_buffer_t* rb) { return WSE_NOT_IMPL; (void)s, (void)rb; }
static ws_error_t wsi_parse_heap_acs(ws_state_t* s, wsi_read_buffer_t* rb) { return WSE_NOT_IMPL; (void)s, (void)rb; }
static ws_error_t wsi_parse_io(ws_state_t* s, wsi_read_buffer_t* rb) { return WSE_NOT_IMPL; (void)s, (void)rb; }
static ws_error_t wsi_parse_flow_ctrl(ws_state_t* s, wsi_read_buffer_t* rb) { return WSE_NOT_IMPL; (void)s, (void)rb; }

ws_error_t ws_compile(
    ws_state_t** sptr,
    void* src, ws_read_t rdr,
    ws_alloc_t alloc, void* udata
) {
    wsi_read_buffer_t rb[1] = {0};
    ws_state_t* state;
    ws_error_t ec;

    if (!sptr || !rdr || !alloc) return WSE_INVAL_ARG;
    *sptr = NULL;
    rb->func = rdr;
    rb->ud   = src;

    state = alloc(NULL, sizeof *state, udata);
    if (!state) return WSE_NO_MEMORY;
    memset(state, 0, sizeof *state);
    state->alloc = alloc;
    state->udata = udata;

    state->instr = alloc(NULL, WSC_INIT_INSTR_CAP, udata);
    if (!state->instr) WSM_THROW(WSE_NO_MEMORY);

    while (1) {
        switch (wsi_rb_get(rb)) {
            case WSA_SPACE:
                ec = wsi_parse_stk_manip(state, rb);
                if (ec) goto error;
                break;

            case WSA_TAB:
                switch (wsi_rb_get(rb)) {
                    case WSA_SPACE:
                        ec = wsi_parse_arith(state, rb);
                        if (ec) goto error;
                        break;

                    case WSA_TAB:
                        ec = wsi_parse_heap_acs(state, rb);
                        if (ec) goto error;
                        break;

                    case WSA_LF:
                        ec = wsi_parse_io(state, rb);
                        if (ec) goto error;
                        break;

                    case WSA_EOF: WSM_THROW(WSE_INCOMPL_INSTR);
                } break;

            case WSA_LF:
                ec = wsi_parse_flow_ctrl(state, rb);
                if (ec) goto error;
                break;

            case WSA_EOF: goto loop_exit;
        }
    }
loop_exit:

    return WSE_OK;
error:
    alloc(state->instr, 0, udata);
    alloc(state, 0, udata);
    return ec;
}