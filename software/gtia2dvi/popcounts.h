#ifndef POPCOUNTS_H
#define POPCOUNTS_H


static inline __force_inline uint32_t popcount_hakmem(uint32_t mask)
{
    uint32_t y;

    y = (mask >> 1) & 033333333333;
    y = mask - y - ((y >> 1) & 033333333333);
    return ((y + (y >> 3)) & 030707070707) % 63;
}

static inline __force_inline uint32_t popcount2(uint32_t n) // 0xac
{
    n = (n & 0x55555555) + ((n >> 1) & 0x55555555);  // (1) add 2 bits
    n = (n & 0x33333333) + ((n >> 2) & 0x33333333);  // (2) add 4 bits
    n = (n & 0x0F0F0F0F) + ((n >> 4) & 0x0F0F0F0F);  // (3)
    n = (n & 0x00FF00FF) + ((n >> 8) & 0x00FF00FF);  // (4)
    n = (n & 0x0000FFFF) + ((n >> 16) & 0x0000FFFF); // (5)
    return n;
}

static inline __force_inline uint32_t popcount4(uint32_t x) // 0x8c
{
    uint32_t m1 = 0x55555555;
    uint32_t m2 = 0x03030303;
    x -= (x >> 1) & m1;
    x = (x & m2) + ((x >> 2) & m2) + ((x >> 4) & m2) + ((x >> 6) & m2);
    x += x >> 8;
    return (x + (x >> 16)) & 0x3f;
}

static inline __force_inline uint32_t popcount3(uint32_t x) // 0x84
{
    uint32_t m1 = 0x55555555;
    uint32_t m2 = 0x33333333;
    uint32_t m4 = 0x0f0f0f0f;
    x -= (x >> 1) & m1;
    x = (x & m2) + ((x >> 2) & m2);
    x = (x + (x >> 4)) & m4;
    x += x >> 8;
    return (x + (x >> 16)) & 0x3f;
}

#endif