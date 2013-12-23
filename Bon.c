#include "Bon.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>

typedef struct BonContainerHeader {
	int32_t			capacity;
	int32_t			count;
} BonContainerHeader;

typedef struct BonNameAndOffset {
	BonName			name;
	int32_t			offset;
} BonNameAndOffset;

#define BON_VALUE_TYPE(v)	((int)(*(v) & 0x7ull))
#define BON_VALUE_PTR(v)	((void*)((uint8_t*)(v) + (*((int32_t*)(v) + 1))))

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

static int 
NameCompare(const void * a, const void * b) {
	return ((BonNameAndOffset*)a)->name - ((BonNameAndOffset*)b)->name;
}

/*---------------------------------------------------------------------------*/

BonBool				
BonIsAValidRecord(const BonRecord* br) {
	const char* magicChars;
	if (!br)
		return BON_FALSE;
	magicChars = (const char*)&br->magic;
	if (!(magicChars[0] == 'B' && magicChars[1] == 'O' && magicChars[2] == 'N' && magicChars[3] == ' ')) {
		return BON_FALSE;
	}
	return BON_TRUE;
}

uint32_t			
BonGetRecordSize(const BonRecord* br) {
	return br->recordSize;
}

const char*
BonGetNameString(const BonRecord* br, BonName name) {
	BonContainerHeader*	header		= (BonContainerHeader*)((uint8_t*)&br->nameLookupTableOffset + br->nameLookupTableOffset);
	BonNameAndOffset*	nameLookup	= (BonNameAndOffset*)(&header[1]);
	BonNameAndOffset	key;
	BonNameAndOffset*	item;
	key.name		= name;
	key.offset		= 0;
	item = bsearch(&key, nameLookup, header->count, sizeof(BonNameAndOffset), NameCompare);
	if (!item) {
		return 0;
	}
	return (const char*)((uint8_t*)&item->offset + item->offset);
}

const BonValue*
BonGetRootValue(const BonRecord* br) {
	return &br->rootValue;
}

int				
BonGetValueType(const BonValue* bv) {
	return (int)(uint64_t)(*bv) & 0x7ull;
}

BonBool				
BonIsNullValue(const BonValue* value) {
	return *value == 0x7ull;
}

BonObject			
BonAsObject(const BonValue* bv) {
	BonObject o;
	BonContainerHeader* header = (BonContainerHeader*)BON_VALUE_PTR(bv);
	if (BON_VALUE_TYPE(bv) != BON_VT_OBJECT) {
		assert(0 && "Expected BON_VT_OBJECT");
		o.count		= 0;
		o.names		= 0;
		o.values	= 0;
		return o;
	}
	o.count		= header->count;
	o.values	= (BonValue*)&header[1];
	o.names		= (BonName*)&o.values[o.count];
	return o;
}

BonArray			
BonAsArray(const BonValue* bv) {
	BonArray o;
	BonContainerHeader* header = (BonContainerHeader*)BON_VALUE_PTR(bv);
	if (BON_VALUE_TYPE(bv) != BON_VT_ARRAY) {
		assert(0 && "Expected BON_VT_ARRAY");
		o.count		= 0;
		o.values	= 0;
		return o;
	}
	o.count		= header->count;
	o.values	= (BonValue*)&header[1];
	return o;
}

double				
BonAsNumber(const BonValue* bv) {
	if (BON_VALUE_TYPE(bv) != BON_VT_NUMBER) {
		assert(0 && "Expected BON_VT_NUMBER");
		return 0.0;
	}
	return *(double*)bv;
}

const char*			
BonAsString(const BonValue* bv) {
	if (BON_VALUE_TYPE(bv) != BON_VT_STRING) {
		assert(0 && "Expected BON_VT_STRING");
		return "";
	}
	return (const char*)BON_VALUE_PTR(bv);
}

BonBool				
BonAsBool(const BonValue* bv) {
	if (BON_VALUE_TYPE(bv) != BON_VT_BOOL) {
		assert(0 && "Expected BON_VT_BOOL");
		return BON_FALSE;
	}
	return (*((int32_t*)(bv) + 1));
}

const double*			
BonAsNumberArray(const BonArray* ba) {
	/* TODO: A debug check that all elements are BON_VT_NUMBER */
	return (const double*)ba->values;
}

BonName				
BonCreateName(const char* nameString, size_t nameStringByteCount) {
	return Hash32(nameString, (uint32_t)nameStringByteCount);
}

