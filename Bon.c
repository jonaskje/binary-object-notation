#include "Bon.h"

#pragma warning(push)
#pragma warning(disable : 4001)	/* Single line comments */
#pragma warning(disable : 4820) /* N bytes padding added after data member X */
#include <stdlib.h>
#include <crtdbg.h>
#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable : 4127)	/* Conditional expression is constant */
#pragma warning(disable : 4820) /* N bytes padding added after data member X */
#pragma warning(disable : 4100) /* Unreferenced formal parameter */


/*---------------------------------------------------------------------------*/
/* Utilities */

/*
 * 32-bit murmur3 from:
 * qLibc - http://www.qdecoder.org
 * 
 * Copyright (c) 2010-2012 Seungyoung Kim.
 * All rights reserved.
 */
static uint32_t 
Hash32(const void *data, uint32_t nbytes) {
	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;
	const int nblocks = nbytes / 4;
	const uint32_t *blocks = (const uint32_t *)(data);
	const uint8_t *tail = (const uint8_t *)data + (nblocks * 4);
	uint32_t h = 0;
	int i;
	uint32_t k;

	if (data == 0 || nbytes == 0)
		return 0;

	for (i = 0; i < nblocks; i++) {
		k = blocks[i];

		k *= c1;
		k = (k << 15) | (k >> (32 - 15));
		k *= c2;

		h ^= k;
		h = (h << 13) | (h >> (32 - 13));
		h = (h * 5) + 0xe6546b64;
	}

	k = 0;
	switch (nbytes & 3) {
	case 3:
		k ^= tail[2] << 16;
	case 2:
		k ^= tail[1] << 8;
	case 1:
		k ^= tail[0];
		k *= c1;
		k = (k << 13) | (k >> (32 - 15));
		k *= c2;
		h ^= k;
	};

	h ^= nbytes;

	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}


/*---------------------------------------------------------------------------*/
/* :Reading */

const char*
BonGetNameString(BonRecord* br, BonName name) {
	assert(0);
	return 0;
}

BonName				
BonCreateName(const char* nameString, size_t nameStringByteCount) {
	return Hash32(nameString, (uint32_t)nameStringByteCount);
}

#pragma warning(pop)
