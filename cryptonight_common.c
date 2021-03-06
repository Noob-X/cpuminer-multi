// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Modified for CPUminer by Lucas Jones

#include "cpuminer-config.h"
#include "miner.h"
#include "crypto/c_groestl.h"
#include "crypto/c_blake256.h"
#include "crypto/c_jh.h"
#include "crypto/c_skein.h"
#include "cryptonight.h"

#if defined __unix__ && (!defined __APPLE__)
#include <sys/mman.h>
#elif defined _WIN32
#include <windows.h>
#endif

void do_blake_hash(const void* input, size_t len, char* output) {
    blake256_hash((uint8_t*)output, input, len);
}

void do_groestl_hash(const void* input, size_t len, char* output) {
    groestl(input, len * 8, (uint8_t*)output);
}

void do_jh_hash(const void* input, size_t len, char* output) {
    jh_hash(32 * 8, input, 8 * len, (uint8_t*)output);
}

void do_skein_hash(const void* input, size_t len, char* output) {
    skein_hash(8 * 32, input, 8 * len, (uint8_t*)output);
}

void xor_blocks_dst(const uint64_t *a, const uint64_t *b, uint8_t *dst)
{
#if __x86_64__
	__m128i *av = (__m128i *)a;
	__m128i *bv = (__m128i *)b;
	__m128i *dstv = (__m128i *)dst;

	*dstv = _mm_xor_si128(*av, *bv);
#else
	((uint64_t*) dst)[0] = a[0] ^ b[0];
	((uint64_t*) dst)[1] = a[1] ^ b[1];
#endif
}

void (* const extra_hashes[4])(const void *, size_t, char *) = {do_blake_hash, do_groestl_hash, do_jh_hash, do_skein_hash};

void cryptonight_hash(void* output, const void* input, size_t len, uint64_t height)
{
    struct cryptonight_ctx *ctx = (struct cryptonight_ctx*)malloc(sizeof(struct cryptonight_ctx));
    cryptonight_hash_ctx(output, input, len, ctx, height);
    free(ctx);
}

int scanhash_cryptonight(int thr_id, uint32_t *restrict pdata, int dlen, const uint32_t *restrict ptarget,
			 uint32_t max_nonce, unsigned long *restrict hashes_done,
			 struct cryptonight_ctx *persistentctx, uint64_t height)
{
    uint32_t *nonceptr = (uint32_t*) (((char*)pdata) + 39);
    uint32_t n = *nonceptr - 1;
    const uint32_t first_nonce = n + 1;
    const uint64_t Htarg = ((const uint64_t *)ptarget)[3];
    uint64_t hash[32 / 8] __attribute__((aligned(64)));
	
	do {
		*nonceptr = ++n;
		cryptonight_hash_ctx(hash, pdata, dlen, persistentctx, height);
		if (unlikely(hash[3] < Htarg)) {
			*hashes_done = n - first_nonce + 1;
			return true;
		}
	} while (likely((n <= max_nonce && !work_restart[thr_id].restart)));
    
    *hashes_done = n - first_nonce + 1;
    return 0;
}
