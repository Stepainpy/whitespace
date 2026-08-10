#include <whitespace/whitespace.h>

#include <stdio.h>

int main(void) {
    puts(ws_strerror(WSE_NOT_IMPL));
    return WSE_NOT_IMPL;
}