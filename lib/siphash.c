// SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause)
/*
 * Siphash [1] is a secure hashing function optimized for speed and efficiency
 * in short-message formats such as network packets. It's used in the Linux
 * kernel for generating TCP handshake keys, amidst other things.
 *
 * Remark: Adapted from linux/lib/siphash.c for the purposes of this exercise
 * This implementation tries to be as faithful as possible to the one in the
 * kernel tree.
 *
 * [1] https://www.aumasson.jp/siphash/siphash.pdf
 */

#include <sys/random.h>
#include <stddef.h>
#include "siphash.h"

#define rol64(x, n) \
	(x << n) | (x >> (-n & 31))

#define PREAMBLE(len) \
	uint64_t v0 = SIPHASH_CONST_0; \
	uint64_t v1 = SIPHASH_CONST_1; \
	uint64_t v2 = SIPHASH_CONST_2; \
	uint64_t v3 = SIPHASH_CONST_3; \
	uint64_t b = ((uint64_t)(len)) << 56; \
	v3 ^= key->key[1]; \
	v2 ^= key->key[0]; \
	v1 ^= key->key[1]; \
	v0 ^= key->key[0];

#define SIPROUNDS(n) \
	for (size_t i = 0; i < n; ++i) \
		SIPROUND;

#define POSTAMBLE \
	v3 ^= b; \
	SIPROUNDS(c); \
	v0 ^= b; \
	v2 ^= 0xff; \
	SIPROUNDS(d); \
	return (v0 ^ v1) ^ (v2 ^ v3);

#define SIPROUND \
	SIPHASH_PERMUTATION(v0, v1, v2, v3)

inline siphash_key_t
key_init(void)
{
	siphash_key_t key = {0};
	if (getrandom((void*)&key, sizeof(siphash_key_t), GRND_RANDOM) < 0)
		perror("getrandom");

	return key;
}

uint64_t
siphash_3u32(
    const uint32_t first,
    const uint32_t second,
    const uint32_t third,
    const siphash_key_t *key,
    const size_t c,
    const size_t d
) {
	uint64_t combined = (uint64_t)second << 32 | first;
	PREAMBLE(12)                /* (3 args) * (4-bytes/arg) mod 256 = 12 */

	v3 ^= combined;
	SIPROUNDS(c);
	v0 ^= combined;
	b |= third;

	POSTAMBLE;
}
