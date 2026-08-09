#include "defines.h"
#include "label.h"

#include <string.h>

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

ws_error_t ws_disasm(ws_code_t* c, void* out, ws_wrfn_t wtr) {
    char intbuf[24] = {0};
    ws_int_t integer;
    size_t i, ii, sz;
    if (!c || !wtr) return WSE_INVAL_ARG;

    for (i = 0; i < c->count; i++) {
        wse_instr_t instr = c->instrs[i];
        if (instr != WSI_MARK)
            if (!wtr("    ", 4, 1, out)) return WSE_FAIL_WRITE;
        switch (instr) {
            case WSI_PUSH:
                if (!wtr("push ", 5, 1, out)) return WSE_FAIL_WRITE;
                memcpy(&integer, c->instrs + i + 1, sizeof integer);
                sz = wsi_conv_int(integer, intbuf);
                if (!wtr(intbuf, sz, 1, out)) return WSE_FAIL_WRITE;
                i += sizeof(ws_int_t);
                break;

            case WSI_DUP:
                if (!wtr("dup", 3, 1, out)) return WSE_FAIL_WRITE;
                break;
            case WSI_SWAP:
                if (!wtr("swap", 4, 1, out)) return WSE_FAIL_WRITE;
                break;
            case WSI_DROP:
                if (!wtr("drop", 4, 1, out)) return WSE_FAIL_WRITE;
                break;

            case WSI_COPY:
                if (!wtr("copy ", 5, 1, out)) return WSE_FAIL_WRITE;
                memcpy(&integer, c->instrs + i + 1, sizeof integer);
                sz = wsi_conv_int(integer, intbuf);
                if (!wtr(intbuf, sz, 1, out)) return WSE_FAIL_WRITE;
                i += sizeof(ws_int_t);
                break;
            case WSI_SLIDE:
                if (!wtr("slide ", 6, 1, out)) return WSE_FAIL_WRITE;
                memcpy(&integer, c->instrs + i + 1, sizeof integer);
                sz = wsi_conv_int(integer, intbuf);
                if (!wtr(intbuf, sz, 1, out)) return WSE_FAIL_WRITE;
                i += sizeof(ws_int_t);
                break;

            case WSI_ADD:
                if (!wtr("add", 3, 1, out)) return WSE_FAIL_WRITE;
                break;
            case WSI_SUB:
                if (!wtr("sub", 3, 1, out)) return WSE_FAIL_WRITE;
                break;
            case WSI_MUL:
                if (!wtr("mul", 3, 1, out)) return WSE_FAIL_WRITE;
                break;
            case WSI_DIV:
                if (!wtr("div", 3, 1, out)) return WSE_FAIL_WRITE;
                break;
            case WSI_MOD:
                if (!wtr("mod", 3, 1, out)) return WSE_FAIL_WRITE;
                break;

            case WSI_STORE:
                if (!wtr("store", 5, 1, out)) return WSE_FAIL_WRITE;
                break;
            case WSI_LOAD:
                if (!wtr("load", 4, 1, out)) return WSE_FAIL_WRITE;
                break;

            case WSI_OUT_CHAR:
                if (!wtr("out-char", 8, 1, out)) return WSE_FAIL_WRITE;
                break;
            case WSI_OUT_INT:
                if (!wtr("out-int", 7, 1, out)) return WSE_FAIL_WRITE;
                break;
            case WSI_IN_CHAR:
                if (!wtr("in-char", 7, 1, out)) return WSE_FAIL_WRITE;
                break;
            case WSI_IN_INT:
                if (!wtr("in-int", 6, 1, out)) return WSE_FAIL_WRITE;
                break;

            case WSI_MARK:
                ii = i + 1; i += WSL_BYTE;
                goto write_label;
            case WSI_CALL:
                if (!wtr("call ", 5, 1, out)) return WSE_FAIL_WRITE;
                goto write_label_ext;
            case WSI_GOTO:
                if (!wtr("goto ", 5, 1, out)) return WSE_FAIL_WRITE;
                goto write_label_ext;
            case WSI_IFZR:
                if (!wtr("if-zero ", 8, 1, out)) return WSE_FAIL_WRITE;
                goto write_label_ext;
            case WSI_IFNG:
                if (!wtr("if-neg ", 7, 1, out)) return WSE_FAIL_WRITE;
                goto write_label_ext;
            case WSI_RET:
                if (!wtr("ret", 3, 1, out)) return WSE_FAIL_WRITE;
                break;
            case WSI_EXIT:
                if (!wtr("exit", 4, 1, out)) return WSE_FAIL_WRITE;
                break;

            write_label_ext:
                memcpy(&ii, c->instrs + i + 1, sizeof ii);
                ii -= WSL_BYTE - 1;
                i  += sizeof(size_t);
            write_label: {
                wsi_label_t label; size_t k;
                memcpy(label.parts, c->instrs + ii, WSL_PARTS_BYTE);
                for (k = 0; k < WSL_BITS; k += 2) {
                    wsa_char_t ch = (label.parts[k / WSL_PART_BITS] >> (k % WSL_PART_BITS)) & 3;
                    /**/ if (ch == WSA_SPACE) { if (!wtr("S", 1, 1, out)) return WSE_FAIL_WRITE; }
                    else if (ch == WSA_TAB  ) { if (!wtr("T", 1, 1, out)) return WSE_FAIL_WRITE; }
                    else break;
                }
            } break;

            default:
            case WSI_UNKNOWN:
                if (!wtr("<unknown>", 9, 1, out)) return WSE_FAIL_WRITE;
                break;
        }
        if (instr == WSI_MARK)
            if (!wtr(":", 1, 1, out)) return WSE_FAIL_WRITE;
        if (!wtr("\n", 1, 1, out)) return WSE_FAIL_WRITE;
    }

    return WSE_OK;
}