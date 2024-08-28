#include <cstdio>
#include "CBitBuffer.h"


CBitBuffer::CBitBuffer(unsigned char* buff) {
    m_buff = buff;
    buff_pos = 0;
    bit_pos = 0;
}

unsigned long CBitBuffer::get(unsigned char nCount) {
    unsigned long ret = 0;
    for (unsigned char i = 0; i < nCount; i++) {
        ret = ret * 2 + get();
    }
    return ret;
}

unsigned char CBitBuffer::get() {
    unsigned char ret;
    ret = (m_buff[buff_pos] >> (7 - bit_pos)) & 1;
    bit_pos++;
    if (bit_pos > 7) {
        bit_pos = 0;
        buff_pos++;
    }
    return ret;
}

unsigned int CBitBuffer::get_golomb() {
    int zero_count = 0;
    for (int i = 0; i < 32; i++) {
        if (get() == 1) break;
        zero_count++;
    }
    unsigned int ret = get(zero_count);
    return (1 << zero_count) + ret - 1;
}
