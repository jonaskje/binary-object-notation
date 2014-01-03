/* vi: set ts=8 sts=8 sw=8 et: */
#include "Bon.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>

#define BON_VALUE_TYPE(v)       ((int)(*(v) & 0x7ull))
#define BON_VALUE_PTR(v)        ((void*)((uint8_t*)(v) + (*((int32_t*)(v) + 1))))

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
        uint32_t aname = ((BonNameAndOffset*)a)->name;
        uint32_t bname = ((BonNameAndOffset*)b)->name;
        if (aname < bname) return -1;
        if (bname < aname) return 1;
        return 0;
}

/*---------------------------------------------------------------------------*/

BonBool                         
BonIsAValidRecord(const BonRecord* br, size_t brSizeInBytes) {
        const char* magicChars;
        if (!br) {
                return BON_FALSE;
        }
        if (((uintptr_t)br & (uintptr_t)0x7u) != 0) {                           
                /* BonRecords must be aligned to 8 byte to prevent unaligned access to data in the
                 * record. */
                return BON_FALSE;
        }
        if (brSizeInBytes > 0 && brSizeInBytes < sizeof(BonRecord)) {
                return BON_FALSE;
        }
        magicChars = (const char*)&br->magic;
        if (!(magicChars[0] == 'B' && magicChars[1] == 'O' && magicChars[2] == 'N' && magicChars[3] == ' ')) {
                return BON_FALSE;
        }
        if (brSizeInBytes > 0 && br->recordSize != brSizeInBytes) {
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
        BonContainerHeader*     header          = (BonContainerHeader*)((uint8_t*)&br->nameLookupTableOffset + br->nameLookupTableOffset);
        BonNameAndOffset*       nameLookup      = (BonNameAndOffset*)(&header[1]);
        BonNameAndOffset        key;
        BonNameAndOffset*       item;
        key.name                = name;
        key.offset              = 0;
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
                o.count         = 0;
                o.names         = 0;
                o.values        = 0;
                return o;
        }
        o.count         = header->count;
        o.values        = (BonValue*)&header[1];
        o.names         = (BonName*)&o.values[o.count];
        return o;
}

BonArray                        
BonAsArray(const BonValue* bv) {
        BonArray o;
        BonContainerHeader* header = (BonContainerHeader*)BON_VALUE_PTR(bv);
        if (BON_VALUE_TYPE(bv) != BON_VT_ARRAY) {
                assert(0 && "Expected BON_VT_ARRAY");
                o.count         = 0;
                o.values        = 0;
                return o;
        }
        o.count         = header->count;
        o.values        = (BonValue*)&header[1];
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

BonNumberArray                  
BonAsNumberArray(const BonValue* bv) {
        BonNumberArray o;
        BonContainerHeader* header = (BonContainerHeader*)BON_VALUE_PTR(bv);
        if (BON_VALUE_TYPE(bv) != BON_VT_ARRAY) {
                assert(0 && "Expected BON_VT_ARRAY");
                o.count         = 0;
                o.values        = 0;
                return o;
        }
        o.count         = header->count;
        o.values        = (const double*)&header[1];
        return o;
}

BonName                         
BonCreateName(const char* nameString, size_t nameStringByteCount) {
        return Hash32(nameString, (uint32_t)nameStringByteCount);
}

BonName
BonCreateNameCstr(const char* nameString) {
	return Hash32(nameString, (uint32_t)strlen(nameString));
}


int 
BonFindIndexOfName(const BonName* names, int nameCount, BonName value) {
	const int64_t value64 = (int64_t)value;
	int l = 0;
	int h = nameCount - 1;
	while (l <= h) {
		const int m = (l + h) / 2;
		int64_t diff = (int64_t)names[m] - value64;
		if (diff < 0)
			l = m + 1;
		else if (diff > 0)
			h = m - 1;
		else
			return m;
	}
	return -1;
}

int                             
BonGetMemberValueType(const BonObject* object, BonName name) {
	int i = BonFindIndexOfName(object->names, object->count, name);
	if (i >= 0) {
		return BonGetValueType(&object->values[i]);
	} else {
		return BON_VT_NULL;
	}
}

BonBool                         
BonIsMemberNullValue(const BonObject* object, BonName name) {
	int i = BonFindIndexOfName(object->names, object->count, name);
	return i < 0 || BonIsNullValue(&object->values[i]);
}


BonObject                       
BonMemberAsObject(const BonObject* object, BonName name) {
	int i = BonFindIndexOfName(object->names, object->count, name);
	if (i >= 0) {
		return BonAsObject(&object->values[i]);
	} else {
		const BonObject empty = {0};
		return empty;
	}
}


BonArray                        
BonMemberAsArray(const BonObject* object, BonName name) {
	int i = BonFindIndexOfName(object->names, object->count, name);
	if (i >= 0) {
		return BonAsArray(&object->values[i]);
	} else {
		const BonArray empty = {0};
		return empty;
	}
}


BonNumberArray                  
BonMemberAsNumberArray(const BonObject* object, BonName name) {
	int i = BonFindIndexOfName(object->names, object->count, name);
	if (i >= 0) {
		return BonAsNumberArray(&object->values[i]);
	} else {
		const BonNumberArray empty = {0};
		return empty;
	}
}


double                          
BonMemberAsNumber(const BonObject* object, BonName name) {
	int i = BonFindIndexOfName(object->names, object->count, name);
	if (i >= 0) {
		return BonAsNumber(&object->values[i]);
	} else {
		return 0.0;
	}
}


const char*                     
BonMemberAsString(const BonObject* object, BonName name) {
	int i = BonFindIndexOfName(object->names, object->count, name);
	if (i >= 0) {
		return BonAsString(&object->values[i]);
	} else {
		return "";
	}
}


BonBool                         
BonMemberAsBool(const BonObject* object, BonName name) {
	int i = BonFindIndexOfName(object->names, object->count, name);
	if (i >= 0) {
		return BonAsBool(&object->values[i]);
	} else {
		return BON_FALSE;
	}
}




