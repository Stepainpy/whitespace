#include "defines.h"

static ws_int_t wsi_read_int(wsi_instr_t* src) {
    ws_int_t out = 0; size_t i;
    for (i = 0; i < sizeof out; i++)
        out |= (ws_int_t)src[i] << (i * 8);
    return out;
}

static size_t wsi_conv_int(ws_int_t integer, char* out) {
    int neg = integer < 0, sz;
    char* end = out;

    integer = neg ? -integer : integer;
    do *end++ = '0' + integer % 10; while (integer /= 10);
    if (neg) *end++ = '-';

    sz = end - out;
    for (--end; out < end; ++out, --end) {
        char t = *out; *out = *end; *end = t;
    }

    return sz;
}

ws_error_t ws_disasm(ws_code_t* c, void* output, ws_write_t wtr) {
    char intbuf[24] = {0}; size_t i, sz;
    if (!c || !wtr) return WSE_INVAL_ARG;

    for (i = 0; i < c->count; i++) {
        switch (c->instrs[i]) {
            case WSI_PUSH:
                if (!wtr("push ", 5, 1, output)) return WSE_FAIL_WRITE;
                sz = wsi_conv_int(wsi_read_int(c->instrs + i + 1), intbuf);
                if (!wtr(intbuf, sz, 1, output)) return WSE_FAIL_WRITE;
                i += sizeof(ws_int_t);
                break;

            case WSI_DUP:
                if (!wtr("dup", 3, 1, output)) return WSE_FAIL_WRITE;
                break;
            case WSI_SWAP:
                if (!wtr("swap", 4, 1, output)) return WSE_FAIL_WRITE;
                break;
            case WSI_DROP:
                if (!wtr("drop", 4, 1, output)) return WSE_FAIL_WRITE;
                break;

            case WSI_COPY:
                if (!wtr("copy ", 5, 1, output)) return WSE_FAIL_WRITE;
                sz = wsi_conv_int(wsi_read_int(c->instrs + i + 1), intbuf);
                if (!wtr(intbuf, sz, 1, output)) return WSE_FAIL_WRITE;
                i += sizeof(ws_int_t);
                break;
            case WSI_SLIDE:
                if (!wtr("slide ", 6, 1, output)) return WSE_FAIL_WRITE;
                sz = wsi_conv_int(wsi_read_int(c->instrs + i + 1), intbuf);
                if (!wtr(intbuf, sz, 1, output)) return WSE_FAIL_WRITE;
                i += sizeof(ws_int_t);
                break;

            case WSI_ADD:
                if (!wtr("add", 3, 1, output)) return WSE_FAIL_WRITE;
                break;
            case WSI_SUB:
                if (!wtr("sub", 3, 1, output)) return WSE_FAIL_WRITE;
                break;
            case WSI_MUL:
                if (!wtr("mul", 3, 1, output)) return WSE_FAIL_WRITE;
                break;
            case WSI_DIV:
                if (!wtr("div", 3, 1, output)) return WSE_FAIL_WRITE;
                break;
            case WSI_MOD:
                if (!wtr("mod", 3, 1, output)) return WSE_FAIL_WRITE;
                break;

            case WSI_STORE:
                if (!wtr("store", 5, 1, output)) return WSE_FAIL_WRITE;
                break;
            case WSI_LOAD:
                if (!wtr("load", 4, 1, output)) return WSE_FAIL_WRITE;
                break;

            case WSI_OUT_CHAR:
                if (!wtr("out-char", 8, 1, output)) return WSE_FAIL_WRITE;
                break;
            case WSI_OUT_INT:
                if (!wtr("out-int", 7, 1, output)) return WSE_FAIL_WRITE;
                break;
            case WSI_IN_CHAR:
                if (!wtr("in-char", 7, 1, output)) return WSE_FAIL_WRITE;
                break;
            case WSI_IN_INT:
                if (!wtr("in-int", 6, 1, output)) return WSE_FAIL_WRITE;
                break;

            default:
            case WSI_UNKNOWN:
                if (!wtr("<unknown>", 9, 1, output)) return WSE_FAIL_WRITE;
                break;
        }
        wtr("\n", 1, 1, output);
    }

    return WSE_OK;
}