#include <whitespace/whitespace.h>

const char* ws_strerror(ws_error_t error) {
    switch (error) {
        case WSE_OK: return "no errors";

        case WSE_INVAL_ARG: return "invalid argument";
        case WSE_NO_MEMORY: return "couldn't allocate memory";
        case WSE_NO_SHRINK: return "couldn't shrink memory";
        case WSE_FAIL_WRITE: return "fail write to output";

        case WSE_INVAL_INSTR: return "invalid instruction";
        case WSE_INCOMPL_INSTR: return "incomplete instruction";

        case WSE_INVAL_SIGN: return "sign is incorrect";
        case WSE_INT_OVERFLOW: return "overflow when parsing integer";
        case WSE_INCOMPL_INT: return "unfinished integer";

        case WSE_INVAL_LABEL: return "invalid label name";
        case WSE_TOO_LONG_LABEL: return "label name too long";
        case WSE_INCOMPL_LABEL: return "unfinished label name";
        case WSE_REDEF_LABEL: return "redefinition label";
        case WSE_NODEF_LABEL: return "label not defined";
        case WSE_UNKNOWN_LABEL: return "reference to unknown label";

        case WSE_NOT_IMPL: return "not implemented";
    }
    return "unknown error";
}