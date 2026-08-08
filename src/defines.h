#ifndef WS_DEFINES_H
#define WS_DEFINES_H

#include <whitespace/whitespace.h>

/* Constants and types */

#define WSC_MAX_HEAP_SIZE 1024
#define WSC_MAX_LABEL_SIZE 64

#define WSC_S_CHAR '\x20'
#define WSC_T_CHAR '\x09'
#define WSC_L_CHAR '\x0A'

typedef enum {
    WSA_EOF = 0,
    WSA_SPACE,
    WSA_TAB,
    WSA_LF
} wsa_char_t;

typedef int ws_int_t;

/* Instructions
 * indicator (-o, +p, r) mean:
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
    /* (-0, +0, 0) */ WSI_GOTO /* <label> */, /* [Space][LF]    */
    /* (-1, +0, 1) */ WSI_IFZR /* <label> */, /* [Tab][Space]   */
    /* (-1, +0, 1) */ WSI_IFNG /* <label> */, /* [Tab][Tab]     */
    /* (-0, +0, 0) */ WSI_RET               , /* [Tab][LF]      */
    /* (-0, +0, 0) */ WSI_EXIT              , /* [LF][LF]       */

    WSI_UNKNOWN /* Placeholder for last comma */
} wse_instr_t;

/* Definition of state */

typedef unsigned char wsi_instr_t;

struct ws_code_t {
    wsi_instr_t* instrs; size_t count;
    ws_alloc_t alloc; void* udata;
};

#endif /* WS_DEFINES_H */