#ifndef WS_LABEL_H
#define WS_LABEL_H

#include <whitespace/whitespace.h>

#define WSL_BITS 256
#define WSL_BYTE ((WSL_BITS + 7) / 8)

#define WSL_PART_BITS (WSL_PART_BYTE * 8)
#define WSL_PART_BYTE sizeof(wsl_part_t)

#define WSL_INVAL_PLACE (~(size_t)0)
#define WSL_PARTS ((WSL_BYTE + WSL_PART_BYTE - 1) / WSL_PART_BYTE)

#define WSL_PARTS_BYTE (WSL_PART_BYTE * WSL_PARTS)

#define WSL_MAX_LABEL_COUNT ((size_t)1 << (sizeof(wsl_index_t) * 8))

typedef unsigned short wsl_index_t;
typedef unsigned long wsl_part_t;

typedef struct {
    size_t place, length;
    wsl_part_t parts[WSL_PARTS];
} wsi_label_t;

typedef struct {
    wsi_label_t* labels;
    size_t count, capacity;
    ws_alloc_t fn; void* ud;
} wsl_list_t;

ws_error_t wsl_push(wsl_list_t* list, const wsi_label_t* label);
ws_error_t wsl_get (wsl_list_t* list, const wsi_label_t* label, wsl_index_t* index);
ws_error_t wsl_getx(wsl_list_t* list, const wsi_label_t* label, wsl_index_t* index);

#endif /* WS_LABEL_H */