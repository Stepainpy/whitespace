#include "label.h"

#include <string.h>

WSM_ARRAY_RESERVE(wsl, wsl_list_t, labels)

ws_error_t wsl_push(wsl_list_t* l, const wsi_label_t* lbl) {
    if (l->count >= WSL_MAX_LABEL_COUNT) return WSE_TOO_MANY_LABELS;
    if (wsl_reserve(l, 1)) return WSE_NO_MEMORY;
    memcpy(l->labels + l->count++, lbl, sizeof *lbl);
    return WSE_OK;
}

static int wsl_equal(const wsi_label_t* lhs, const wsi_label_t* rhs) {
    size_t i;
    if (lhs->length != rhs->length) return 0;
    for (i = 0; i < WSL_PARTS; i++)
        if (lhs->parts[i] != rhs->parts[i]) return 0;
    return 1;
}

ws_error_t wsl_get(wsl_list_t* l, const wsi_label_t* lbl, wsl_index_t* index) {
    size_t i; ws_error_t ec;
    for (i = 0; i < l->count; i++)
        if (wsl_equal(lbl, l->labels + i)) { *index = i; return WSE_OK; }
    if ((ec = wsl_push(l, lbl))) return ec;
    l->labels[*index = l->count - 1].place = WSL_INVAL_PLACE;
    return WSE_OK;
}

ws_error_t wsl_getx(wsl_list_t* l, const wsi_label_t* lbl, wsl_index_t* index) {
    size_t i; ws_error_t ec;
    for (i = 0; i < l->count; i++)
        if (wsl_equal(lbl, l->labels + i)) {
            if (l->labels[i].place != WSL_INVAL_PLACE) return WSE_REDEF_LABEL;
            *index = i; return WSE_OK;
        }
    if ((ec = wsl_push(l, lbl))) return ec;
    *index = l->count - 1;
    return WSE_OK;
}