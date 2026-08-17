#ifndef WS_LABEL_H
#define WS_LABEL_H

#include "constants.h"
#include "array.h"

#include <limits.h>

#define WSM_CEIL(a, b) (((a) + (b) - 1) / (b))

#define WSL_BYTES WSM_CEIL(WSC_MAX_LABEL_LENGTH, CHAR_BIT)
#define WSL_PARTS WSM_CEIL(WSL_BYTES, sizeof(wsl_part_t))

#define WSL_MAX_LABEL_COUNT ((size_t)1 << (sizeof(wsl_index_t) * CHAR_BIT))

#define WSL_PART_BITS (sizeof(wsl_part_t) * CHAR_BIT)

#define WSL_INVAL_PLACE (~(size_t)0)

typedef unsigned short wsl_index_t;
typedef unsigned long wsl_part_t;

typedef struct {
    size_t place, length;
    wsl_part_t parts[WSL_PARTS];
} wsi_label_t;

typedef WSM_ARRAY_STRUCT(wsi_label_t, labels) wsl_list_t;

ws_error_t wsl_push(wsl_list_t* list, const wsi_label_t* label);
ws_error_t wsl_get (wsl_list_t* list, const wsi_label_t* label, wsl_index_t* index);
ws_error_t wsl_getx(wsl_list_t* list, const wsi_label_t* label, wsl_index_t* index);

#endif /* WS_LABEL_H */