//
// Created by degor on 27.08.2023.
//

#ifndef FRANCA2_HEX_EX_H
#define FRANCA2_HEX_EX_H
#include "arrays.h"

void print_hex(const arrays::arrayv<u8>& bytes) {
    for(var i = (size_t)0; i < bytes.count; i++) {
        printf("%02x ", bytes[i]);
        if (i % 16 == 15) printf("\n");
    }
    printf("\n");
}

#endif //FRANCA2_HEX_EX_H
