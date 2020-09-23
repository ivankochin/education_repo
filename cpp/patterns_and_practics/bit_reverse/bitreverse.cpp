#include <bitset>
#include <iostream>
#include <cstdio>
#include <chrono>

namespace gcc_builtins {
    inline uint16_t byte_swap(uint16_t x){ return __builtin_bswap16(x); }
    inline uint32_t byte_swap(uint32_t x){ return __builtin_bswap32(x); }
    inline uint64_t byte_swap(uint64_t x){ return __builtin_bswap64(x); }
}

inline uint16_t bswap_bitreverse(uint16_t v) {
    v = ((v >> 1) & 0x5555) | ((v & 0x5555) << 1);
    v = ((v >> 2) & 0x3333) | ((v & 0x3333) << 2);
    v = ((v >> 4) & 0x0F0F) | ((v & 0x0F0F) << 4);
    v = gcc_builtins::byte_swap(v);
    return v;
}

inline uint32_t bswap_bitreverse(uint32_t v) {
    v = ((v >> 1) & 0x55555555) | ((v & 0x55555555) << 1 );
    v = ((v >> 2) & 0x33333333) | ((v & 0x33333333) << 2 );
    v = ((v >> 4) & 0x0F0F0F0F) | ((v & 0x0F0F0F0F) << 4 );
    v = gcc_builtins::byte_swap(v);
    return v;
}

inline uint64_t bswap_bitreverse(uint64_t v) {
    v = ((v >> 1 ) & 0x5555555555555555) | ((v & 0x5555555555555555) << 1 );
    v = ((v >> 2 ) & 0x3333333333333333) | ((v & 0x3333333333333333) << 2 );
    v = ((v >> 4 ) & 0x0F0F0F0F0F0F0F0F) | ((v & 0x0F0F0F0F0F0F0F0F) << 4 );
    v = gcc_builtins::byte_swap(v);
    return v;
}


inline uint8_t shift_bitreverse(uint8_t v) {
    v = ((v >> 1) & 0x55) | ((v & 0x55) << 1);
    v = ((v >> 2) & 0x33) | ((v & 0x33) << 2);
    v = ( v >> 4        ) | ( v         << 4);
    return v;
}

inline uint16_t shift_bitreverse(uint16_t v) {
    v = ((v >> 1) & 0x5555) | ((v & 0x5555) << 1);
    v = ((v >> 2) & 0x3333) | ((v & 0x3333) << 2);
    v = ((v >> 4) & 0x0F0F) | ((v & 0x0F0F) << 4);
    v = ( v >> 8          ) | ( v << 8          );
    return v;
}

inline uint32_t shift_bitreverse(uint32_t v) {
    v = ((v >> 1) & 0x55555555) | ((v & 0x55555555) << 1 );
    v = ((v >> 2) & 0x33333333) | ((v & 0x33333333) << 2 );
    v = ((v >> 4) & 0x0F0F0F0F) | ((v & 0x0F0F0F0F) << 4 );
    v = ((v >> 8) & 0x00FF00FF) | ((v & 0x00FF00FF) << 8 );
    v = ( v >> 16             ) | ( v               << 16);
    return v;
}

inline uint64_t shift_bitreverse(uint64_t v) {
    v = ((v >> 1 ) & 0x5555555555555555) | ((v & 0x5555555555555555) << 1 );
    v = ((v >> 2 ) & 0x3333333333333333) | ((v & 0x3333333333333333) << 2 );
    v = ((v >> 4 ) & 0x0F0F0F0F0F0F0F0F) | ((v & 0x0F0F0F0F0F0F0F0F) << 4 );
    v = ((v >> 8 ) & 0x00FF00FF00FF00FF) | ((v & 0x00FF00FF00FF00FF) << 8 );
    v = ((v >> 16) & 0x0000FFFF0000FFFF) | ((v & 0x0000FFFF0000FFFF) << 16);
    v = ( v >> 32                      ) | ( v                       << 32);
    return v;
}

namespace  llvm_builtins {
    inline uint8_t  bitreverse(uint8_t  x) { return __builtin_bitreverse8 (x); }
    inline uint16_t bitreverse(uint16_t x) { return __builtin_bitreverse16(x); }
    inline uint32_t bitreverse(uint32_t x) { return __builtin_bitreverse32(x); }
    inline uint64_t bitreverse(uint64_t x) { return __builtin_bitreverse64(x); }
}

template<typename T>
struct reverse {
    static const T byte_table[256];
};

template<typename T>
const T reverse<T>::byte_table[256] = {
    0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
    0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
    0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
    0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
    0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
    0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
    0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
    0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
    0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
    0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
    0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
    0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
    0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
    0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
    0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
    0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

inline unsigned char reverse_byte(unsigned char src) {
    return reverse<unsigned char>::byte_table[src];
}

template<typename T>
T lookup_table_bitreverse(T src) {
    T dst;
    unsigned char *original = (unsigned char *) &src;
    unsigned char *reversed = (unsigned char *) &dst;

    for( int i = sizeof(T)-1; i >= 0; i-- )
        reversed[i] = reverse_byte( original[sizeof(T)-i-1] );

    return dst;
}

int main() {
    volatile uint16_t i = 199;
    std::cout<<std::bitset<64>(i)<<std::endl;

    i = llvm_builtins::bitreverse(i);
    i = lookup_table_bitreverse(i);
    i = shift_bitreverse(i);
    i = bswap_bitreverse(i);

    std::cout<<std::bitset<64>(i)<<std::endl;
    return 0;
}
