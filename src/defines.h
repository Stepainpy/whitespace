#ifndef WS_DEFINES_H
#define WS_DEFINES_H

#include <whitespace/whitespace.h>

#define WSC_MAX_HEAP_SIZE 1024

#define WSM_STACK_STRUCT(type) \
    struct { type* values; size_t index, capacity; }

/* Instructions description
 * Indicator (-o, +p, r) mean:
 *   -o is how many pops from stack
 *   +p is how many pushes onto stack
 *    r is how many required in stack
 */
typedef enum {
    /* [Space] - Stack manipulation */
    /* (-0, +1,  0 ) */ WSI_PUSH  /*  <integer>  */, /* [Space]      */
    /* (-0, +1,  1 ) */ WSI_DUP                    , /* [LF][Space]  */
    /* (-2, +2,  2 ) */ WSI_SWAP                   , /* [LF][Tab]    */
    /* (-1, +0,  1 ) */ WSI_DROP                   , /* [LF][LF]     */
    /* (-0, +1,  n ) */ WSI_COPY  /* <integer> n */, /* [Tab][Sapce] */
    /* (-n, +0, n+1) */ WSI_SLIDE /* <integer> n */, /* [Tab][LF]    */

    /* [Tab][Space] - Arithmetic */
    /* (-2, +1, 2) */ WSI_ADD, /* [Space][Space] */
    /* (-2, +1, 2) */ WSI_SUB, /* [Space][Tab]   */
    /* (-2, +1, 2) */ WSI_MUL, /* [Space][LF]    */
    /* (-2, +1, 2) */ WSI_DIV, /* [Tab][Space]   */
    /* (-2, +1, 2) */ WSI_MOD, /* [Tab][Tab]     */

    /* [Tab][Tab] - Heap access */
    /* (-2, +0, 2) */ WSI_STORE, /* [Space] */
    /* (-1, +1, 1) */ WSI_LOAD , /* [Tab]   */

    /* [Tab][LF] - I/O */
    /* (-1, +0, 1) */ WSI_OUT_CHAR, /* [Space][Space] */
    /* (-1, +0, 1) */ WSI_OUT_INT , /* [Space][Tab]   */
    /* (-1, +0, 1) */ WSI_IN_CHAR , /* [Tab][Space]   */
    /* (-1, +0, 1) */ WSI_IN_INT  , /* [Tab][Tab]     */

    /* [LF] - Flow control */
    /* (-0, +0, 0) */ WSI_MARK /* <label> */, /* [Space][Space] */
    /* (-0, +0, 0) */ WSI_CALL /* <label> */, /* [Space][Tab]   */
    /* (-0, +0, 0) */ WSI_JUMP /* <label> */, /* [Space][LF]    */
    /* (-1, +0, 1) */ WSI_JZER /* <label> */, /* [Tab][Space]   */
    /* (-1, +0, 1) */ WSI_JNEG /* <label> */, /* [Tab][Tab]     */
    /* (-0, +0, 0) */ WSI_RET               , /* [Tab][LF]      */
    /* (-0, +0, 0) */ WSI_EXIT              , /* [LF][LF]       */

    /* Aliases for [Space], [Tab] and [LF] */
    WSI_SPACE, WSI_TAB, WSI_LF
} wse_instr_t;

typedef int ws_int_t;
typedef unsigned char ws_instr_t;

typedef WSM_STACK_STRUCT(ws_int_t) ws_value_stack_t;
typedef WSM_STACK_STRUCT(  size_t) ws_call_stack_t;

struct ws_state_t {
    ws_instr_t* instrs;
    size_t ip, count;

    ws_value_stack_t stack;
    ws_call_stack_t calls;
    ws_int_t* heap;
};

#endif /* WS_DEFINES_H */