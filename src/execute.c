#include "defines.h"

#include <string.h>

#define EOF (-1)

#define WSC_INIT_DATA_CAP 16
#define WSC_INIT_CALL_CAP 16
#define WSC_INIT_PAGE_CAP 1

#define WSC_HEAP_PAGE_SIZE 64

#define WSS_STRUCT(type) \
    struct { type* values; size_t count, capacity; ws_alloc_t fn; void* ud; }

#define WSS_RESERVE(sname, stype) \
static ws_error_t wss_##sname##_reserve(stype* s, size_t need) {  \
    size_t newcap; void* newptr;                                  \
    if (s->count + need <= s->capacity) return WSE_OK;            \
                                                                  \
    newcap = s->capacity;                                         \
    while (s->count + need > newcap)                              \
        newcap = (newcap * 207 + 127) / 128;                      \
                                                                  \
    newptr = s->fn(s->values, sizeof *s->values * newcap, s->ud); \
    if (!newptr) return WSE_NO_MEMORY;                            \
                                                                  \
    s->values   = newptr;                                         \
    s->capacity = newcap;                                         \
    return WSE_OK;                                                \
}                                                                 \

#define WSS_PUSH(sname, stype, type) \
static ws_error_t wss_##sname##_push(stype* s, type value) { \
    if (wss_##sname##_reserve(s, 1)) return WSE_NO_MEMORY;   \
    s->values[s->count++] = value;                           \
    return WSE_OK;                                           \
}                                                            \

typedef WSS_STRUCT(ws_int_t) wss_data_t;
WSS_RESERVE(data, wss_data_t)
WSS_PUSH   (data, wss_data_t, ws_int_t)

typedef WSS_STRUCT(size_t) wss_call_t;
WSS_RESERVE(call, wss_call_t)
WSS_PUSH   (call, wss_call_t, size_t)

typedef struct {
    size_t base;
    ws_int_t data[WSC_HEAP_PAGE_SIZE];
} wsh_page_t;

typedef struct {
    wsh_page_t* pages;
    size_t count, capacity;
    ws_alloc_t fn; void* ud;
} wsi_heap_t;

static ws_error_t wsh_reserve(wsi_heap_t* h) {
    size_t newcap; void* newptr;
    if (h->count + 1 <= h->capacity) return WSE_OK;

    newcap = h->capacity;
    while (h->count + 1 > newcap)
        newcap = (newcap * 207 + 127) / 128;

    newptr = h->fn(h->pages, sizeof *h->pages * newcap, h->ud);
    if (!newptr) return WSE_NO_MEMORY;

    h->pages    = newptr;
    h->capacity = newcap;
    return WSE_OK;
}

static ws_int_t* wsh_get(wsi_heap_t* h, size_t address) {
    wsh_page_t* page; size_t i;

    for (i = 0; i < h->count; i++) {
        page = h->pages + i;
        if (page->base <= address && address < page->base + WSC_HEAP_PAGE_SIZE)
            goto get_address;
    }

    if (wsh_reserve(h)) return NULL;
    page = h->pages + h->count++;
    page->base = address - address % WSC_HEAP_PAGE_SIZE;

get_address:
    return page->data + address - page->base;
}

static ws_error_t wsi_put_utf8(void* out, ws_wrfn_t wtr, ws_int_t chr) {
    unsigned char buf[4], cnt = 0;
    /*  */ if (chr < 0x00080) {
        buf[cnt++] = chr;
    } else if (chr < 0x00800) {
        buf[cnt++] = 0xC0 | (chr >> 6 & 0x1F);
        buf[cnt++] = 0x80 | (chr      & 0x3F);
    } else if (chr < 0x10000) {
        buf[cnt++] = 0xE0 | (chr >> 12 & 0x0F);
        buf[cnt++] = 0x80 | (chr >>  6 & 0x3F);
        buf[cnt++] = 0x80 | (chr       & 0x3F);
    } else {
        buf[cnt++] = 0xF8 | (chr >> 18 & 0x07);
        buf[cnt++] = 0x80 | (chr >> 12 & 0x3F);
        buf[cnt++] = 0x80 | (chr >>  6 & 0x3F);
        buf[cnt++] = 0x80 | (chr       & 0x3F);
    }
    return wtr(buf, cnt, 1, out) ? WSE_OK : WSE_FAIL_WRITE;
}

static ws_error_t wsi_put_int(void* out, ws_wrfn_t wtr, ws_int_t Int) {
    char buf[24], *begin = buf, *end = buf; int neg = Int < 0, cnt;
    Int = neg ? -Int : Int;

    do *end++ = '0' + Int % 10; while (Int /= 10);
    if (neg) *end++ = '-';
    cnt = end - begin;
    for (--end; begin < end; ++begin, --end) {
        char t = *begin; *begin = *end; *end = t;
    }

    return wtr(buf, cnt, 1, out) ? WSE_OK : WSE_FAIL_WRITE;
}

static ws_error_t wsi_get_utf8(void* in, ws_rdfn_t rdr, ws_int_t* chr) {
    unsigned char buf[4], cnt;
    if (!rdr(buf, 1, 1, in)) return WSE_FAIL_READ;

    /**/ if (        buf[0] && buf[0] < 0x80) cnt = 1;
    else if (0xC0 <= buf[0] && buf[0] < 0xDF) cnt = 2;
    else if (0xE0 <= buf[0] && buf[0] < 0xEF) cnt = 3;
    else if (0xF0 <= buf[0] && buf[0] < 0xF7) cnt = 4;
    else return WSE_INVAL_UTF8;

    if (cnt > 1 && !rdr(buf + 1, cnt - 1, 1, in)) return WSE_FAIL_READ;
    if (cnt > 1 && !(0x80 <= buf[1] && buf[1] <= 0xBF)) return WSE_INVAL_UTF8;
    if (cnt > 2 && !(0x80 <= buf[2] && buf[2] <= 0xBF)) return WSE_INVAL_UTF8;
    if (cnt > 3 && !(0x80 <= buf[3] && buf[3] <= 0xBF)) return WSE_INVAL_UTF8;

    switch (cnt) {
        case 1: *chr = buf[0]; break;
        case 2: *chr = (ws_int_t)(buf[0] & 0x1F) << 6
                     | (ws_int_t)(buf[1] & 0x3F); break;
        case 3: *chr = (ws_int_t)(buf[0] & 0x0F) << 12
                     | (ws_int_t)(buf[1] & 0x3F) << 6
                     | (ws_int_t)(buf[2] & 0x3F); break;
        case 4: *chr = (ws_int_t)(buf[0] & 0x0F) << 18
                     | (ws_int_t)(buf[1] & 0x3F) << 12
                     | (ws_int_t)(buf[2] & 0x3F) << 6
                     | (ws_int_t)(buf[3] & 0x3F); break;
    }

    return WSE_OK;
}

static ws_error_t wsi_get_int(void* in, ws_rdfn_t rdr, ws_int_t* Int) {
#define GETC() (rdr(&chr, 1, 1, in) ? chr : (chr = EOF))
    int chr = 0; int neg = 0; *Int = 0;

    do GETC(); while (chr == ' ' || chr == '\t' || chr == '\n');

    /**/ if (chr == '-') { neg = 1; GETC(); }
    else if (chr == '+') { neg = 0; GETC(); }

    if (chr < '0' || '9' < chr) return WSE_NO_INT;
    while ('0' <= chr && chr <= '9') {
        *Int = 10 * *Int + (chr - '0'); GETC();
    }
    if (neg) *Int = -*Int;

    return WSE_OK;
}

#define WSM_THROW(err_code) \
    do { ec = err_code; goto error; } while (0)

ws_error_t ws_execute(ws_code_t* c,
    void*  in, ws_rdfn_t rdr,
    void* out, ws_wrfn_t wtr
) {
    wss_data_t dstk[1] = {0};
    wss_call_t cstk[1] = {0};
    wsi_heap_t heap[1] = {0};
    wsl_index_t lbl_idx;
    ws_int_t a, b, *ptr;
    ws_int_t arg, idx;
    ws_error_t ec;
    size_t i;

    if (!c || !c->instrs || !rdr || !wtr) return WSE_INVAL_ARG;

    dstk->fn = cstk->fn = heap->fn = c->alloc;
    dstk->ud = cstk->ud = heap->ud = c->udata;

    dstk->values = dstk->fn(NULL, sizeof *dstk->values *
        (dstk->capacity = WSC_INIT_DATA_CAP), dstk->ud);
    if (!dstk->values) WSM_THROW(WSE_NO_MEMORY);

    cstk->values = cstk->fn(NULL, sizeof *cstk->values *
        (cstk->capacity = WSC_INIT_CALL_CAP), cstk->ud);
    if (!cstk->values) WSM_THROW(WSE_NO_MEMORY);

    heap-> pages = heap->fn(NULL, sizeof *heap-> pages *
        (heap->capacity = WSC_INIT_PAGE_CAP), heap->ud);
    if (!heap-> pages) WSM_THROW(WSE_NO_MEMORY);

    for (i = 0; i < c->icnt; i++)
        switch (c->instrs[i]) {

            /* Stack manipulation */

            case WSI_PUSH:
                memcpy(&arg, c->instrs + i + 1, sizeof arg); i += sizeof arg;
                if (wss_data_push(dstk, arg)) WSM_THROW(WSE_STACK_OVERFLOW);
                break;

            case WSI_DUP:
                if (dstk->count < 1) WSM_THROW(WSE_NOT_ENOUGH);
                if (wss_data_push(dstk, dstk->values[dstk->count - 1]))
                    WSM_THROW(WSE_STACK_OVERFLOW);
                break;

            case WSI_SWAP:
                if (dstk->count < 2) WSM_THROW(WSE_NOT_ENOUGH);
                arg = dstk->values[dstk->count - 1];
                dstk->values[dstk->count - 1] = dstk->values[dstk->count - 2];
                dstk->values[dstk->count - 2] = arg;
                break;

            case WSI_DROP:
                if (dstk->count < 1) WSM_THROW(WSE_NOT_ENOUGH);
                dstk->count -= 1;
                break;

            case WSI_COPY:
                memcpy(&arg, c->instrs + i + 1, sizeof arg); i += sizeof arg;
                if (dstk->count <= (size_t)arg) WSM_THROW(WSE_OUT_OF_BOUNDS);
                if (wss_data_push(dstk, dstk->values[dstk->count - 1 - arg]))
                    WSM_THROW(WSE_STACK_OVERFLOW);
                break;

            case WSI_SLIDE:
                if (dstk->count < 1) WSM_THROW(WSE_NOT_ENOUGH);
                memcpy(&arg, c->instrs + i + 1, sizeof arg); i += sizeof arg;
                arg = (size_t)arg < dstk->count - 1 ? (size_t)arg : dstk->count - 1;
                dstk->values[dstk->count - 1 - arg] = dstk->values[dstk->count - 1];
                dstk->count -= arg;
                break;

            /* Arithmetic */

            case WSI_ADD:
                if (dstk->count < 2) WSM_THROW(WSE_NOT_ENOUGH);
                b = dstk->values[--dstk->count];
                a = dstk->values[--dstk->count];
                if (wss_data_push(dstk, a + b))
                    WSM_THROW(WSE_STACK_OVERFLOW);
                break;

            case WSI_SUB:
                if (dstk->count < 2) WSM_THROW(WSE_NOT_ENOUGH);
                b = dstk->values[--dstk->count];
                a = dstk->values[--dstk->count];
                if (wss_data_push(dstk, a - b))
                    WSM_THROW(WSE_STACK_OVERFLOW);
                break;

            case WSI_MUL:
                if (dstk->count < 2) WSM_THROW(WSE_NOT_ENOUGH);
                b = dstk->values[--dstk->count];
                a = dstk->values[--dstk->count];
                if (wss_data_push(dstk, a * b))
                    WSM_THROW(WSE_STACK_OVERFLOW);
                break;

            case WSI_DIV:
                if (dstk->count < 2) WSM_THROW(WSE_NOT_ENOUGH);
                b = dstk->values[--dstk->count];
                a = dstk->values[--dstk->count];
                if (wss_data_push(dstk, a / b))
                    WSM_THROW(WSE_STACK_OVERFLOW);
                break;

            case WSI_MOD:
                if (dstk->count < 2) WSM_THROW(WSE_NOT_ENOUGH);
                b = dstk->values[--dstk->count];
                a = dstk->values[--dstk->count];
                if (wss_data_push(dstk, a % b))
                    WSM_THROW(WSE_STACK_OVERFLOW);
                break;

            /* Heap access */

            case WSI_STORE:
                if (dstk->count < 2) WSM_THROW(WSE_NOT_ENOUGH);
                arg = dstk->values[--dstk->count];
                idx = dstk->values[--dstk->count];
                if (!(ptr = wsh_get(heap, idx))) WSM_THROW(WSE_SIGSEGV);
                *ptr = arg;
                break;

            case WSI_LOAD:
                if (dstk->count < 1) WSM_THROW(WSE_NOT_ENOUGH);
                idx = dstk->values[--dstk->count];
                if (!(ptr = wsh_get(heap, idx))) WSM_THROW(WSE_SIGSEGV);
                if (wss_data_push(dstk, *ptr)) WSM_THROW(WSE_STACK_OVERFLOW);
                break;

            /* I/O */

            case WSI_OUT_CHAR:
                if (dstk->count < 1) WSM_THROW(WSE_NOT_ENOUGH);
                arg = dstk->values[--dstk->count];
                if (arg < 0 || 0x10FFFF < arg) WSM_THROW(WSE_INVAL_UTF8);
                if ((ec = wsi_put_utf8(out, wtr, arg))) goto error;
                break;

            case WSI_OUT_INT:
                if (dstk->count < 1) WSM_THROW(WSE_NOT_ENOUGH);
                arg = dstk->values[--dstk->count];
                if ((ec = wsi_put_int(out, wtr, arg))) goto error;
                break;

            case WSI_IN_CHAR:
                if (dstk->count < 1) WSM_THROW(WSE_NOT_ENOUGH);
                idx = dstk->values[--dstk->count];
                if (!(ptr = wsh_get(heap, idx))) WSM_THROW(WSE_SIGSEGV);
                if ((ec = wsi_get_utf8(in, rdr, &arg))) goto error;
                *ptr = arg;
                break;

            case WSI_IN_INT:
                if (dstk->count < 1) WSM_THROW(WSE_NOT_ENOUGH);
                idx = dstk->values[--dstk->count];
                if (!(ptr = wsh_get(heap, idx))) WSM_THROW(WSE_SIGSEGV);
                if ((ec = wsi_get_int(in, rdr, &arg))) goto error;
                *ptr = arg;
                break;

            /* Flow control */

            case WSI_MARK:
                i += sizeof(wsl_index_t);
                break;

            case WSI_CALL:
                memcpy(&lbl_idx, c->instrs + i + 1, sizeof lbl_idx); i += sizeof lbl_idx;
                if (wss_call_push(cstk, i)) WSM_THROW(WSE_CALL_OVERFLOW);
                i = c->labels[lbl_idx].place + sizeof(wsl_index_t);
                break;

            case WSI_GOTO:
                memcpy(&lbl_idx, c->instrs + i + 1, sizeof lbl_idx); i += sizeof lbl_idx;
                i = c->labels[lbl_idx].place + sizeof(wsl_index_t);
                break;

            case WSI_IFZR:
                if (dstk->count < 1) WSM_THROW(WSE_NOT_ENOUGH);
                memcpy(&lbl_idx, c->instrs + i + 1, sizeof lbl_idx); i += sizeof lbl_idx;
                arg = dstk->values[--dstk->count];
                if (arg == 0) i = c->labels[lbl_idx].place + sizeof(wsl_index_t);
                break;

            case WSI_IFNG:
                if (dstk->count < 1) WSM_THROW(WSE_NOT_ENOUGH);
                memcpy(&lbl_idx, c->instrs + i + 1, sizeof lbl_idx); i += sizeof lbl_idx;
                arg = dstk->values[--dstk->count];
                if (arg < 0) i = c->labels[lbl_idx].place + sizeof(wsl_index_t);
                break;

            case WSI_RET:
                if (cstk->count < 1) WSM_THROW(WSE_CALL_UNDERFLOW);
                i = cstk->values[--cstk->count];
                break;

            case WSI_EXIT:
                goto loop_exit;

            /* Other */

            default:
            case WSI_UNKNOWN:
                WSM_THROW(WSE_UNKNOWN_INSTR);
        }
    WSM_THROW(WSE_OUT_OF_CODE);

loop_exit:
    ec = WSE_OK;
error:
    heap->fn(heap-> pages, 0, heap->ud);
    cstk->fn(cstk->values, 0, cstk->ud);
    dstk->fn(dstk->values, 0, dstk->ud);
    return ec;
}