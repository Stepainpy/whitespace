#ifndef WS_TEMPLATE_ARRAY_H
#define WS_TEMPLATE_ARRAY_H

#include <whitespace/whitespace.h>

#define WSM_ARRAY_STRUCT(value_type, value_field) \
    struct { value_type* value_field; size_t count, capacity; ws_alloc_t fn; void* ud; }

#define WSM_ARRAY_INIT(array, value_field, init_capacity, error_case) do { \
    array->capacity = init_capacity;                              \
    array->value_field = array->fn(NULL,                          \
        sizeof *array->value_field * array->capacity, array->ud); \
    if (!array->value_field) error_case;                          \
} while (0)

#define WSM_ARRAY_SHRINK(array, value_field, error_case) do { \
    void* newptr = array->fn(array->value_field,               \
        sizeof *array->value_field * array->count, array->ud); \
    if (array->count > 0 && !newptr) error_case;               \
    array->value_field = newptr;                               \
} while (0)

#define WSM_ARRAY_PUSH(prefix, array_type, value_field, value_type) \
static ws_error_t prefix##_push(array_type* array, value_type value) { \
    if (prefix##_reserve(array, 1)) return WSE_NO_MEMORY;              \
    array->value_field[array->count++] = value;                        \
    return WSE_OK;                                                     \
}                                                                      \

#define WSM_ARRAY_RESERVE(prefix, array_type, value_field) \
static ws_error_t prefix##_reserve(array_type* array, size_t require) { \
    size_t newcap; void* newptr;                                        \
    if (array->count + require <= array->capacity) return WSE_OK;       \
                                                                        \
    newcap = array->capacity;                                           \
    while (array->count + require > newcap)                             \
        newcap = (newcap * 207 + 127) / 128;                            \
                                                                        \
    newptr = array->fn(array->value_field,                              \
        sizeof *array->value_field * newcap, array->ud);                \
    if (!newptr) return WSE_NO_MEMORY;                                  \
                                                                        \
    array->value_field = newptr;                                        \
    array->capacity    = newcap;                                        \
    return WSE_OK;                                                      \
}                                                                       \

#endif /* WS_TEMPLATE_ARRAY_H */