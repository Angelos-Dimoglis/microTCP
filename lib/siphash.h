#ifndef _SIPHASH_H
#define _SIPHASH_H

#include <stdint.h>
#include <stdio.h>

#define SIPHASH_CONST_0 0x736f6d6570736575ULL
#define SIPHASH_CONST_1 0x646f72616e646f6dULL
#define SIPHASH_CONST_2 0x6c7967656e657261ULL
#define SIPHASH_CONST_3 0x7465646279746573ULL

#define SIPHASH_PERMUTATION(a, b, c, d) ( \
	(a) += (b), (b) = rol64((b), 13), (b) ^= (a), (b) = rol64((b), 17), \
	(b) ^= (a), (a) = rol64((a), 32), (c) += (d), (d) = rol64((d), 16), \
	(d) ^= (c), (c) += (a), \
	(a) ^= (c), (c) = rol64((c), 32), (a) += (d), (d) = rol64((d), 21), \
	(d) ^= (a) \
)

typedef struct {
	uint64_t key[2];
} siphash_key_t;

siphash_key_t key_init(void);

uint64_t siphash_3u32(
    const uint32_t first,
    const uint32_t second,
    const uint32_t third,
    const siphash_key_t *key,
    const size_t c,
    const size_t d
);

#endif /* _SIPHASH_H */
