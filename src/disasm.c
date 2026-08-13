#include "defines.h"

#include <string.h>

#define ZEROS "00""000""000""000""000""000""000"

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

static size_t wsi_conv_address(size_t address, char* out) {
    size_t sz; char* end = out;

    do *end++ = '0' + address % 10; while (address /= 10);
    sz = end - out;
    for (--end; out < end; ++out, --end) {
        char t = *out; *out = *end; *end = t;
    }

    return sz;
}

static size_t wsi_address_width(size_t max) {
    size_t width = 1;
    while (max /= 10) ++width;
    return width;
}

ws_error_t ws_disasm(ws_code_t* c, void* out, ws_wrfn_t wtr) {
    char intbuf[24] = {0};
    wsl_index_t lbl_idx;
    ws_int_t integer;
    size_t i, j, sz, aw;

    if (!c || !c->instrs || !wtr) return WSE_INVAL_ARG;

    aw = wsi_address_width(c->icnt - 1);

    for (i = 0; i < c->icnt; i++) {
        wse_instr_t instr = c->instrs[i];

        sz = wsi_conv_address(i, intbuf);
        if (sz < aw && !wtr(ZEROS, aw - sz, 1, out)) return WSE_FAIL_WRITE;
        if (!wtr(intbuf, sz, 1, out)) return WSE_FAIL_WRITE;
        if (!wtr(" ", 1, 1, out)) return WSE_FAIL_WRITE;

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
                goto write_label;
            case WSI_CALL:
                if (!wtr("call ", 5, 1, out)) return WSE_FAIL_WRITE;
                goto write_label;
            case WSI_GOTO:
                if (!wtr("goto ", 5, 1, out)) return WSE_FAIL_WRITE;
                goto write_label;
            case WSI_IFZR:
                if (!wtr("if-zero ", 8, 1, out)) return WSE_FAIL_WRITE;
                goto write_label;
            case WSI_IFNG:
                if (!wtr("if-neg ", 7, 1, out)) return WSE_FAIL_WRITE;
                goto write_label;
            case WSI_RET:
                if (!wtr("ret", 3, 1, out)) return WSE_FAIL_WRITE;
                break;
            case WSI_EXIT:
                if (!wtr("exit", 4, 1, out)) return WSE_FAIL_WRITE;
                break;

            write_label:
                memcpy(&lbl_idx, c->instrs + i + 1, sizeof lbl_idx); i += sizeof lbl_idx;
                for (j = 0; j < c->labels[lbl_idx].length; j++) {
                    wsl_part_t bit = (c->labels[lbl_idx].parts[j / WSL_PART_BITS] >> (j % WSL_PART_BITS)) & 1;
                    if (!wtr("ST" + bit, 1, 1, out)) return WSE_FAIL_WRITE;
                }
                break;

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