#include "parse.h"

#include <string.h>

typedef enum {
    WSA_SPACE = '\x20',
    WSA_TAB   = '\x09',
    WSA_LF    = '\x0A',
    WSA_EOF   = 0
} wsa_char_t;

static wsa_char_t wsi_rb_get(wsi_read_buffer_t* rdbuf) {
retry:
    if (rdbuf->index < rdbuf->count) {
        wsa_char_t ch = rdbuf->window[rdbuf->index++];
        if (ch == WSA_SPACE || ch == WSA_TAB || ch == WSA_LF)
            return ch;
        else
            goto retry;
    } else {
        rdbuf->index = 0;
        rdbuf->count = rdbuf->fn(
            rdbuf->window, 1, WSC_READ_BUFFER_SIZE, rdbuf->ud);
        if (rdbuf->count) goto retry;
        return WSA_EOF;
    }
}

WSM_ARRAY_RESERVE(wsi_instr, wsi_instrs_t, instrs)
WSM_ARRAY_PUSH   (wsi_instr, wsi_instrs_t, instrs, wsi_instr_t)

static ws_error_t wsi_instr_push_data(wsi_instrs_t* array, void* data, size_t size) {
    if (wsi_instr_reserve(array, size)) return WSE_NO_MEMORY;
    memcpy(array->instrs + array->count, data, size); array->count += size;
    return WSE_OK;
}

static ws_error_t wsp_integer(wsi_read_buffer_t* rb, ws_int_t* out) {
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

static ws_error_t wsp_label(wsi_read_buffer_t* rb, wsi_label_t* lbl) {
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

static ws_error_t wsp_stk_manip(wsi_read_buffer_t* rb, wsi_instrs_t* a) {
    ws_int_t arg; ws_error_t ec;

    switch (wsi_rb_get(rb)) {
        case WSA_SPACE:
            if ((ec = wsp_integer(rb, &arg))) return ec;
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
                    if ((ec = wsp_integer(rb, &arg))) return ec;
                    if (arg < 0) return WSE_INVAL_PARAM;
                    if ((ec = wsi_instr_push(a, WSI_COPY))) return ec;
                    if ((ec = wsi_instr_push_data(a, &arg, sizeof arg))) return ec;
                    break;

                case WSA_LF:
                    if ((ec = wsp_integer(rb, &arg))) return ec;
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

static ws_error_t wsp_arith(wsi_read_buffer_t* rb, wsi_instrs_t* a) {
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

static ws_error_t wsp_heap_acs(wsi_read_buffer_t* rb, wsi_instrs_t* a) {
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

static ws_error_t wsp_io(wsi_read_buffer_t* rb, wsi_instrs_t* a) {
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

static ws_error_t wsp_flow_ctrl(wsi_read_buffer_t* rb, wsi_instrs_t* a, wsl_list_t* ll) {
    ws_error_t ec; wsi_label_t label; wsl_index_t index;

    switch (wsi_rb_get(rb)) {
        case WSA_SPACE:
            switch (wsi_rb_get(rb)) {
                case WSA_SPACE: {
                    if ((ec = wsp_label(rb, &label))) return ec;
                    if ((ec = wsl_getx(ll, &label, &index))) return ec;
                    if ((ec = wsi_instr_push(a, WSI_MARK))) return ec;
                    ll->labels[index].place = a->count - 1;
                    if ((ec = wsi_instr_push_data(a, &index, sizeof index))) return ec;
                } break;

                case WSA_TAB: {
                    if ((ec = wsp_label(rb, &label))) return ec;
                    if ((ec = wsl_get(ll, &label, &index))) return ec;
                    if ((ec = wsi_instr_push(a, WSI_CALL))) return ec;
                    if ((ec = wsi_instr_push_data(a, &index, sizeof index))) return ec;
                } break;

                case WSA_LF: {
                    if ((ec = wsp_label(rb, &label))) return ec;
                    if ((ec = wsl_get(ll, &label, &index))) return ec;
                    if ((ec = wsi_instr_push(a, WSI_GOTO))) return ec;
                    if ((ec = wsi_instr_push_data(a, &index, sizeof index))) return ec;
                } break;

                case WSA_EOF: return WSE_INCOMPL_INSTR;
            } break;

        case WSA_TAB:
            switch (wsi_rb_get(rb)) {
                case WSA_SPACE: {
                    if ((ec = wsp_label(rb, &label))) return ec;
                    if ((ec = wsl_get(ll, &label, &index))) return ec;
                    if ((ec = wsi_instr_push(a, WSI_IFZR))) return ec;
                    if ((ec = wsi_instr_push_data(a, &index, sizeof index))) return ec;
                } break;

                case WSA_TAB: {
                    if ((ec = wsp_label(rb, &label))) return ec;
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

ws_error_t wsi_parse_instr(wsi_read_buffer_t* rb, int* le, wsi_instrs_t* a, wsl_list_t* l) {
    ws_error_t ec;

    switch (wsi_rb_get(rb)) {
        case WSA_SPACE:
            if ((ec = wsp_stk_manip(rb, a))) return ec;
            break;

        case WSA_TAB:
            switch (wsi_rb_get(rb)) {
                case WSA_SPACE:
                    if ((ec = wsp_arith(rb, a))) return ec;
                    break;

                case WSA_TAB:
                    if ((ec = wsp_heap_acs(rb, a))) return ec;
                    break;

                case WSA_LF:
                    if ((ec = wsp_io(rb, a))) return ec;
                    break;

                case WSA_EOF: return WSE_INCOMPL_INSTR;
            } break;

        case WSA_LF:
            if ((ec = wsp_flow_ctrl(rb, a, l))) return ec;
            break;

        case WSA_EOF:
            *le = 1;
            return WSE_OK;
    }

    return WSE_OK;
}