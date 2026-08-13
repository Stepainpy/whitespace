#include <whitespace/whitespace.h>

const char* ws_strerror(ws_error_t error) {
    switch (error) {
        case WSE_OK: return "no errors";

        case WSE_INVAL_ARG: return "invalid argument";
        case WSE_NO_MEMORY: return "couldn't allocate memory";
        case WSE_NO_SHRINK: return "couldn't shrink memory";
        case WSE_FAIL_READ: return "fail read from input";
        case WSE_FAIL_WRITE: return "fail write to output";

        case WSE_INVAL_PARAM: return "invalid parameter";
        case WSE_INVAL_INSTR: return "invalid instruction";
        case WSE_INCOMPL_INSTR: return "incomplete instruction";
        case WSE_UNKNOWN_INSTR: return "unknown instruction";

        case WSE_INVAL_SIGN: return "sign is incorrect";
        case WSE_INT_OVERFLOW: return "overflow when parsing integer";
        case WSE_INCOMPL_INT: return "unfinished integer";

        case WSE_INVAL_LABEL: return "invalid label name";
        case WSE_TOO_LONG_LABEL: return "label name too long";
        case WSE_TOO_MANY_LABELS: return "too many labels defined";
        case WSE_INCOMPL_LABEL: return "unfinished label name";
        case WSE_REDEF_LABEL: return "redefinition label";
        case WSE_NODEF_LABEL: return "label not defined";
        case WSE_UNKNOWN_LABEL: return "reference to unknown label";

        case WSE_STACK_OVERFLOW: return "overflow value's stack";
        case WSE_CALL_OVERFLOW: return "overflow call stack";
        case WSE_CALL_UNDERFLOW: return "return from non-function code";
        case WSE_NOT_ENOUGH: return "not enough values in stack";
        case WSE_OUT_OF_BOUNDS: return "out of bounds stack indexes";

        case WSE_SIGSEGV: return "segmentation fault";

        case WSE_INVAL_UTF8: return "invalid UTF-8 byte sequence";
        case WSE_NO_INT: return "not single characters for integer";

        case WSE_OUT_OF_CODE: return "out of defined instructions";
        case WSE_NOT_IMPL: return "not implemented";
    }
    return "unknown error";
}