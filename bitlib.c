#include <string.h>
#include <stdint.h>
#include <errno.h>

#define __user
#define min(a,b) ((a)<(b)?(a):(b))
#define BITS_PER_LONG (8 * 4)
#define BITOP_WORD(nr)		((nr) / BITS_PER_LONG)
#define BITOP_LE_SWIZZLE	((BITS_PER_LONG-1) & ~0x7)
#define BITMAP_LAST_WORD_MASK(nbits)					\
(									\
	((nbits) % BITS_PER_LONG) ?					\
		(1UL<<((nbits) % BITS_PER_LONG))-1 : ~0UL		\
)
#define BIT(nr)			(1UL << (nr))
#define BIT_MASK(nr)		(1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)		((nr) / BITS_PER_LONG)
#define BITS_PER_BYTE		8
#define BITS_TO_LONGS(nr)	DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define ALIGN(x,a)		__ALIGN_MASK(x,(typeof(x))(a)-1)
#define __ALIGN_MASK(x,mask)	(((x)+(mask))&~(mask))
#define PTR_ALIGN(p, a)		((typeof(p))ALIGN((unsigned long)(p), (a)))
#define IS_ALIGNED(x, a)		(((x) & ((typeof(x))(a) - 1)) == 0)

typedef uint8_t u8;
typedef uint32_t u32;

unsigned int hweight32(unsigned int w)
{
	unsigned int res = w - ((w >> 1) & 0x55555555);
	res = (res & 0x33333333) + ((res >> 2) & 0x33333333);
	res = (res + (res >> 4)) & 0x0F0F0F0F;
	res = res + (res >> 8);
	return (res + (res >> 16)) & 0x000000FF;
}

unsigned int hweight_long(unsigned long w)
{
    return hweight32(w);
}

/**
 * __ffs - find first set bit in word
 * @word: The word to search
 *
 * Undefined if no bit exists, so code should check against 0 first.
 */
static inline unsigned long __ffs(unsigned long word)
{
	asm("bsf %1,%0"
		: "=r" (word)
		: "rm" (word));
	return word;
}

/**
 * ffz - find first zero bit in word
 * @word: The word to search
 *
 * Undefined if no zero exists, so code should check against ~0UL first.
 */
static inline unsigned long ffz(unsigned long word)
{
	asm("bsf %1,%0"
		: "=r" (word)
		: "r" (~word));
	return word;
}

unsigned long find_next_bit(const unsigned long *addr, unsigned long size,
			    unsigned long offset)
{
	const unsigned long *p = addr + BITOP_WORD(offset);
	unsigned long result = offset & ~(BITS_PER_LONG-1);
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset %= BITS_PER_LONG;
	if (offset) {
		tmp = *(p++);
		tmp &= (~0UL << offset);
		if (size < BITS_PER_LONG)
			goto found_first;
		if (tmp)
			goto found_middle;
		size -= BITS_PER_LONG;
		result += BITS_PER_LONG;
	}
	while (size & ~(BITS_PER_LONG-1)) {
		if ((tmp = *(p++)))
			goto found_middle;
		result += BITS_PER_LONG;
		size -= BITS_PER_LONG;
	}
	if (!size)
		return result;
	tmp = *p;

found_first:
	tmp &= (~0UL >> (BITS_PER_LONG - size));
	if (tmp == 0UL)		/* Are any bits set? */
		return result + size;	/* Nope. */
found_middle:
	return result + __ffs(tmp);
}

/*
 * This implementation of find_{first,next}_zero_bit was stolen from
 * Linus' asm-alpha/bitops.h.
 */
unsigned long find_next_zero_bit(const unsigned long *addr, unsigned long size,
				 unsigned long offset)
{
	const unsigned long *p = addr + BITOP_WORD(offset);
	unsigned long result = offset & ~(BITS_PER_LONG-1);
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset %= BITS_PER_LONG;
	if (offset) {
		tmp = *(p++);
		tmp |= ~0UL >> (BITS_PER_LONG - offset);
		if (size < BITS_PER_LONG)
			goto found_first;
		if (~tmp)
			goto found_middle;
		size -= BITS_PER_LONG;
		result += BITS_PER_LONG;
	}
	while (size & ~(BITS_PER_LONG-1)) {
		if (~(tmp = *(p++)))
			goto found_middle;
		result += BITS_PER_LONG;
		size -= BITS_PER_LONG;
	}
	if (!size)
		return result;
	tmp = *p;

found_first:
	tmp |= ~0UL << size;
	if (tmp == ~0UL)	/* Are any bits zero? */
		return result + size;	/* Nope. */
found_middle:
	return result + ffz(tmp);
}
/*
 * Find the first set bit in a memory region.
 */
unsigned long find_first_bit(const unsigned long *addr, unsigned long size)
{
	const unsigned long *p = addr;
	unsigned long result = 0;
	unsigned long tmp;

	while (size & ~(BITS_PER_LONG-1)) {
		if ((tmp = *(p++)))
			goto found;
		result += BITS_PER_LONG;
		size -= BITS_PER_LONG;
	}
	if (!size)
		return result;

	tmp = (*p) & (~0UL >> (BITS_PER_LONG - size));
	if (tmp == 0UL)		/* Are any bits set? */
		return result + size;	/* Nope. */
found:
	return result + __ffs(tmp);
}

/*
 * Find the first cleared bit in a memory region.
 */
unsigned long find_first_zero_bit(const unsigned long *addr, unsigned long size)
{
	const unsigned long *p = addr;
	unsigned long result = 0;
	unsigned long tmp;

	while (size & ~(BITS_PER_LONG-1)) {
		if (~(tmp = *(p++)))
			goto found;
		result += BITS_PER_LONG;
		size -= BITS_PER_LONG;
	}
	if (!size)
		return result;

	tmp = (*p) | (~0UL << size);
	if (tmp == ~0UL)	/* Are any bits zero? */
		return result + size;	/* Nope. */
found:
	return result + ffz(tmp);
}

/*
 * bitmaps provide an array of bits, implemented using an an
 * array of unsigned longs.  The number of valid bits in a
 * given bitmap does _not_ need to be an exact multiple of
 * BITS_PER_LONG.
 *
 * The possible unused bits in the last, partially used word
 * of a bitmap are 'don't care'.  The implementation makes
 * no particular effort to keep them zero.  It ensures that
 * their value will not affect the results of any operation.
 * The bitmap operations that return Boolean (bitmap_empty,
 * for example) or scalar (bitmap_weight, for example) results
 * carefully filter out these unused bits from impacting their
 * results.
 *
 * These operations actually hold to a slightly stronger rule:
 * if you don't input any bitmaps to these ops that have some
 * unused bits set, then they won't output any set unused bits
 * in output bitmaps.
 *
 * The byte ordering of bitmaps is more natural on little
 * endian architectures.  See the big-endian headers
 * include/asm-ppc64/bitops.h and include/asm-s390/bitops.h
 * for the best explanations of this ordering.
 */

int __bitmap_empty(const unsigned long *bitmap, int bits)
{
	int k, lim = bits/BITS_PER_LONG;
	for (k = 0; k < lim; ++k)
		if (bitmap[k])
			return 0;

	if (bits % BITS_PER_LONG)
		if (bitmap[k] & BITMAP_LAST_WORD_MASK(bits))
			return 0;

	return 1;
}

int __bitmap_full(const unsigned long *bitmap, int bits)
{
	int k, lim = bits/BITS_PER_LONG;
	for (k = 0; k < lim; ++k)
		if (~bitmap[k])
			return 0;

	if (bits % BITS_PER_LONG)
		if (~bitmap[k] & BITMAP_LAST_WORD_MASK(bits))
			return 0;

	return 1;
}

int __bitmap_equal(const unsigned long *bitmap1,
		const unsigned long *bitmap2, int bits)
{
	int k, lim = bits/BITS_PER_LONG;
	for (k = 0; k < lim; ++k)
		if (bitmap1[k] != bitmap2[k])
			return 0;

	if (bits % BITS_PER_LONG)
		if ((bitmap1[k] ^ bitmap2[k]) & BITMAP_LAST_WORD_MASK(bits))
			return 0;

	return 1;
}

void __bitmap_complement(unsigned long *dst, const unsigned long *src, int bits)
{
	int k, lim = bits/BITS_PER_LONG;
	for (k = 0; k < lim; ++k)
		dst[k] = ~src[k];

	if (bits % BITS_PER_LONG)
		dst[k] = ~src[k] & BITMAP_LAST_WORD_MASK(bits);
}

/**
 * __bitmap_shift_right - logical right shift of the bits in a bitmap
 *   @dst : destination bitmap
 *   @src : source bitmap
 *   @shift : shift by this many bits
 *   @bits : bitmap size, in bits
 *
 * Shifting right (dividing) means moving bits in the MS -> LS bit
 * direction.  Zeros are fed into the vacated MS positions and the
 * LS bits shifted off the bottom are lost.
 */
void __bitmap_shift_right(unsigned long *dst,
			const unsigned long *src, int shift, int bits)
{
	int k, lim = BITS_TO_LONGS(bits), left = bits % BITS_PER_LONG;
	int off = shift/BITS_PER_LONG, rem = shift % BITS_PER_LONG;
	unsigned long mask = (1UL << left) - 1;
	for (k = 0; off + k < lim; ++k) {
		unsigned long upper, lower;

		/*
		 * If shift is not word aligned, take lower rem bits of
		 * word above and make them the top rem bits of result.
		 */
		if (!rem || off + k + 1 >= lim)
			upper = 0;
		else {
			upper = src[off + k + 1];
			if (off + k + 1 == lim - 1 && left)
				upper &= mask;
		}
		lower = src[off + k];
		if (left && off + k == lim - 1)
			lower &= mask;
		dst[k] = upper << (BITS_PER_LONG - rem) | lower >> rem;
		if (left && k == lim - 1)
			dst[k] &= mask;
	}
	if (off)
		memset(&dst[lim - off], 0, off*sizeof(unsigned long));
}


/**
 * __bitmap_shift_left - logical left shift of the bits in a bitmap
 *   @dst : destination bitmap
 *   @src : source bitmap
 *   @shift : shift by this many bits
 *   @bits : bitmap size, in bits
 *
 * Shifting left (multiplying) means moving bits in the LS -> MS
 * direction.  Zeros are fed into the vacated LS bit positions
 * and those MS bits shifted off the top are lost.
 */

void __bitmap_shift_left(unsigned long *dst,
			const unsigned long *src, int shift, int bits)
{
	int k, lim = BITS_TO_LONGS(bits), left = bits % BITS_PER_LONG;
	int off = shift/BITS_PER_LONG, rem = shift % BITS_PER_LONG;
	for (k = lim - off - 1; k >= 0; --k) {
		unsigned long upper, lower;

		/*
		 * If shift is not word aligned, take upper rem bits of
		 * word below and make them the bottom rem bits of result.
		 */
		if (rem && k > 0)
			lower = src[k - 1];
		else
			lower = 0;
		upper = src[k];
		if (left && k == lim - 1)
			upper &= (1UL << left) - 1;
		dst[k + off] = lower  >> (BITS_PER_LONG - rem) | upper << rem;
		if (left && k + off == lim - 1)
			dst[k + off] &= (1UL << left) - 1;
	}
	if (off)
		memset(dst, 0, off*sizeof(unsigned long));
}

int __bitmap_and(unsigned long *dst, const unsigned long *bitmap1,
				const unsigned long *bitmap2, int bits)
{
	int k;
	int nr = BITS_TO_LONGS(bits);
	unsigned long result = 0;

	for (k = 0; k < nr; k++)
		result |= (dst[k] = bitmap1[k] & bitmap2[k]);
	return result != 0;
}

void __bitmap_or(unsigned long *dst, const unsigned long *bitmap1,
				const unsigned long *bitmap2, int bits)
{
	int k;
	int nr = BITS_TO_LONGS(bits);

	for (k = 0; k < nr; k++)
		dst[k] = bitmap1[k] | bitmap2[k];
}

void __bitmap_xor(unsigned long *dst, const unsigned long *bitmap1,
				const unsigned long *bitmap2, int bits)
{
	int k;
	int nr = BITS_TO_LONGS(bits);

	for (k = 0; k < nr; k++)
		dst[k] = bitmap1[k] ^ bitmap2[k];
}

int __bitmap_andnot(unsigned long *dst, const unsigned long *bitmap1,
				const unsigned long *bitmap2, int bits)
{
	int k;
	int nr = BITS_TO_LONGS(bits);
	unsigned long result = 0;

	for (k = 0; k < nr; k++)
		result |= (dst[k] = bitmap1[k] & ~bitmap2[k]);
	return result != 0;
}

int __bitmap_intersects(const unsigned long *bitmap1,
				const unsigned long *bitmap2, int bits)
{
	int k, lim = bits/BITS_PER_LONG;
	for (k = 0; k < lim; ++k)
		if (bitmap1[k] & bitmap2[k])
			return 1;

	if (bits % BITS_PER_LONG)
		if ((bitmap1[k] & bitmap2[k]) & BITMAP_LAST_WORD_MASK(bits))
			return 1;
	return 0;
}

int __bitmap_subset(const unsigned long *bitmap1,
				const unsigned long *bitmap2, int bits)
{
	int k, lim = bits/BITS_PER_LONG;
	for (k = 0; k < lim; ++k)
		if (bitmap1[k] & ~bitmap2[k])
			return 0;

	if (bits % BITS_PER_LONG)
		if ((bitmap1[k] & ~bitmap2[k]) & BITMAP_LAST_WORD_MASK(bits))
			return 0;
	return 1;
}

int __bitmap_weight(const unsigned long *bitmap, int bits)
{
	int k, w = 0, lim = bits/BITS_PER_LONG;

	for (k = 0; k < lim; k++)
		w += hweight_long(bitmap[k]);

	if (bits % BITS_PER_LONG)
		w += hweight_long(bitmap[k] & BITMAP_LAST_WORD_MASK(bits));

	return w;
}

#define BITMAP_FIRST_WORD_MASK(start) (~0UL << ((start) % BITS_PER_LONG))

void bitmap_set(unsigned long *map, int start, int nr)
{
	unsigned long *p = map + BIT_WORD(start);
	const int size = start + nr;
	int bits_to_set = BITS_PER_LONG - (start % BITS_PER_LONG);
	unsigned long mask_to_set = BITMAP_FIRST_WORD_MASK(start);

	while (nr - bits_to_set >= 0) {
		*p |= mask_to_set;
		nr -= bits_to_set;
		bits_to_set = BITS_PER_LONG;
		mask_to_set = ~0UL;
		p++;
	}
	if (nr) {
		mask_to_set &= BITMAP_LAST_WORD_MASK(size);
		*p |= mask_to_set;
	}
}

void bitmap_clear(unsigned long *map, int start, int nr)
{
	unsigned long *p = map + BIT_WORD(start);
	const int size = start + nr;
	int bits_to_clear = BITS_PER_LONG - (start % BITS_PER_LONG);
	unsigned long mask_to_clear = BITMAP_FIRST_WORD_MASK(start);

	while (nr - bits_to_clear >= 0) {
		*p &= ~mask_to_clear;
		nr -= bits_to_clear;
		bits_to_clear = BITS_PER_LONG;
		mask_to_clear = ~0UL;
		p++;
	}
	if (nr) {
		mask_to_clear &= BITMAP_LAST_WORD_MASK(size);
		*p &= ~mask_to_clear;
	}
}

/*
 * bitmap_find_next_zero_area - find a contiguous aligned zero area
 * @map: The address to base the search on
 * @size: The bitmap size in bits
 * @start: The bitnumber to start searching at
 * @nr: The number of zeroed bits we're looking for
 * @align_mask: Alignment mask for zero area
 *
 * The @align_mask should be one less than a power of 2; the effect is that
 * the bit offset of all zero areas this function finds is multiples of that
 * power of 2. A @align_mask of 0 means no alignment is required.
 */
unsigned long bitmap_find_next_zero_area(unsigned long *map,
					 unsigned long size,
					 unsigned long start,
					 unsigned int nr,
					 unsigned long align_mask)
{
	unsigned long index, end, i;
again:
	index = find_next_zero_bit(map, size, start);

	/* Align allocation */
	index = __ALIGN_MASK(index, align_mask);

	end = index + nr;
	if (end > size)
		return end;
	i = find_next_bit(map, end, index);
	if (i < end) {
		start = i + 1;
		goto again;
	}
	return index;
}

/*
 * Bitmap printing & parsing functions: first version by Bill Irwin,
 * second version by Paul Jackson, third by Joe Korty.
 */

#define CHUNKSZ				32
#define nbits_to_hold_value(val)	fls(val)
#define unhex(c)			(isdigit(c) ? (c - '0') : (toupper(c) - 'A' + 10))
#define BASEDEC 10		/* fancier cpuset lists input in decimal */


enum {
	REG_OP_ISFREE,		/* true if region is all zero bits */
	REG_OP_ALLOC,		/* set all bits in region */
	REG_OP_RELEASE,		/* clear all bits in region */
};

static int __reg_op(unsigned long *bitmap, int pos, int order, int reg_op)
{
	int nbits_reg;		/* number of bits in region */
	int index;		/* index first long of region in bitmap */
	int offset;		/* bit offset region in bitmap[index] */
	int nlongs_reg;		/* num longs spanned by region in bitmap */
	int nbitsinlong;	/* num bits of region in each spanned long */
	unsigned long mask;	/* bitmask for one long of region */
	int i;			/* scans bitmap by longs */
	int ret = 0;		/* return value */

	/*
	 * Either nlongs_reg == 1 (for small orders that fit in one long)
	 * or (offset == 0 && mask == ~0UL) (for larger multiword orders.)
	 */
	nbits_reg = 1 << order;
	index = pos / BITS_PER_LONG;
	offset = pos - (index * BITS_PER_LONG);
	nlongs_reg = BITS_TO_LONGS(nbits_reg);
	nbitsinlong = min(nbits_reg,  BITS_PER_LONG);

	/*
	 * Can't do "mask = (1UL << nbitsinlong) - 1", as that
	 * overflows if nbitsinlong == BITS_PER_LONG.
	 */
	mask = (1UL << (nbitsinlong - 1));
	mask += mask - 1;
	mask <<= offset;

	switch (reg_op) {
	case REG_OP_ISFREE:
		for (i = 0; i < nlongs_reg; i++) {
			if (bitmap[index + i] & mask)
				goto done;
		}
		ret = 1;	/* all bits in region free (zero) */
		break;

	case REG_OP_ALLOC:
		for (i = 0; i < nlongs_reg; i++)
			bitmap[index + i] |= mask;
		break;

	case REG_OP_RELEASE:
		for (i = 0; i < nlongs_reg; i++)
			bitmap[index + i] &= ~mask;
		break;
	}
done:
	return ret;
}

/**
 * bitmap_find_free_region - find a contiguous aligned mem region
 *	@bitmap: array of unsigned longs corresponding to the bitmap
 *	@bits: number of bits in the bitmap
 *	@order: region size (log base 2 of number of bits) to find
 *
 * Find a region of free (zero) bits in a @bitmap of @bits bits and
 * allocate them (set them to one).  Only consider regions of length
 * a power (@order) of two, aligned to that power of two, which
 * makes the search algorithm much faster.
 *
 * Return the bit offset in bitmap of the allocated region,
 * or -errno on failure.
 */
int bitmap_find_free_region(unsigned long *bitmap, int bits, int order)
{
	int pos, end;		/* scans bitmap by regions of size order */

	for (pos = 0 ; (end = pos + (1 << order)) <= bits; pos = end) {
		if (!__reg_op(bitmap, pos, order, REG_OP_ISFREE))
			continue;
		__reg_op(bitmap, pos, order, REG_OP_ALLOC);
		return pos;
	}
	return -ENOMEM;
}

/**
 * bitmap_release_region - release allocated bitmap region
 *	@bitmap: array of unsigned longs corresponding to the bitmap
 *	@pos: beginning of bit region to release
 *	@order: region size (log base 2 of number of bits) to release
 *
 * This is the complement to __bitmap_find_free_region() and releases
 * the found region (by clearing it in the bitmap).
 *
 * No return value.
 */
void bitmap_release_region(unsigned long *bitmap, int pos, int order)
{
	__reg_op(bitmap, pos, order, REG_OP_RELEASE);
}

/**
 * bitmap_allocate_region - allocate bitmap region
 *	@bitmap: array of unsigned longs corresponding to the bitmap
 *	@pos: beginning of bit region to allocate
 *	@order: region size (log base 2 of number of bits) to allocate
 *
 * Allocate (set bits in) a specified region of a bitmap.
 *
 * Return 0 on success, or %-EBUSY if specified region wasn't
 * free (not all bits were zero).
 */
int bitmap_allocate_region(unsigned long *bitmap, int pos, int order)
{
	if (!__reg_op(bitmap, pos, order, REG_OP_ISFREE))
		return -EBUSY;
	__reg_op(bitmap, pos, order, REG_OP_ALLOC);
	return 0;
}
