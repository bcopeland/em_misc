#include <stdint.h>

unsigned long bitmap_find_next_zero_area(unsigned long *map,
					 unsigned long size,
					 unsigned long start,
					 unsigned int nr,
					 unsigned long align_mask);

static inline int bit_set(uint8_t *bitmap, int bit)
{
    return !!(bitmap[bit >> 3] & (1 << (bit & 7)));
}

static inline void set_bit(uint8_t *bitmap, int bit, int val)
{
    bitmap[bit >> 3] |= (val << (bit & 7));
}

static inline int fls(int f)
{
    int order;
    for (order = 0; f; f >>= 1, order++) ;

    return order;
}

static inline int ilog2(int f)
{
    return fls(f) - 1;
}

static inline int is_power_of_two(int f)
{
    return (f & (f-1)) == 0;
}

static inline int hyperfloor(int f)
{
    return 1 << (fls(f) - 1);
}

static inline int hyperceil(int f)
{
    return 1 << fls(f-1);
}
