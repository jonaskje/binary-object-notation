#pragma once
/**
* @file
*/

#include <stdint.h>
#include <stdlib.h>

typedef uint64_t		BonValue;
typedef uint32_t		BonName;
typedef int			BonBool;

#define BON_VT_NUMBER		0
#define BON_VT_BOOL		1
#define BON_VT_STRING		2
#define BON_VT_ARRAY		3
#define BON_VT_OBJECT		4
#define BON_VT_NULL		7

#define BON_FALSE		0
#define BON_TRUE		1

typedef struct BonRecord {
	uint32_t		magic;			/* FourCC('B', 'O', 'N', ' ') */
	uint32_t		reserved;		/* Must be 0 */
	uint32_t		recordSize;		/* Total size of the entire record */
	int32_t			nameLookupTableOffset;	/* Offset to name lookup table relative &nameLookupTableOffset */
	BonValue		rootValue;		/* Variant referencing the root value in the record (an array or an object) */
} BonRecord;

typedef struct BonArrayValue {
	int32_t			type;
	int32_t			offset;
} BonArrayValue;

typedef struct BonObjectValue {
	int32_t			type;
	int32_t			offset;
} BonObjectValue;

typedef struct BonStringValue {
	int32_t			type;
	int32_t			offset;
} BonStringValue;

typedef struct BonContainerHeader {
	int32_t			capacity;
	int32_t			count;
} BonContainerHeader;

typedef struct BonNameAndOffset {
	BonName			name;
	int32_t			offset;
} BonNameAndOffset;


