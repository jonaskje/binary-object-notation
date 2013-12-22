#pragma once

#include <stdint.h>

typedef uint64_t		BonValue;
typedef uint32_t		BonName;
typedef int			BonBool;

typedef struct BonRecord {
	uint32_t		magic;
	uint8_t			version;
	uint8_t			flags;
	uint16_t		reserved0;
	uint32_t		reserved1;
	int32_t			nameLookupTableOffset;		
	BonValue		rootValue;
} BonRecord;

typedef struct BonObject {
	int			count;
	const BonValue*		values;
	const BonName*		names;
} BonObject;

typedef struct BonArray {
	int			count;
	const BonValue*		values;
} BonArray;

#define BON_VT_NUMBER		0
#define BON_VT_BOOL		1
#define BON_VT_STRING		3
#define BON_VT_ARRAY		4
#define BON_VT_OBJECT		5
#define BON_VT_NULL		9

#define BON_FALSE		0
#define BON_TRUE		1

/* Reading */

BonBool				BonIsAValidRecord(const BonRecord* br, size_t sizeBytes);

const char*			BonGetNameString(const BonRecord* br, BonName name);
const BonValue*			BonGetRootValue(const BonRecord* br);

int				BonGetValueType(const BonValue* value);
BonBool				BonIsNullValue(const BonValue* value);

BonObject			BonAsObject(const BonValue* bv);
BonArray			BonAsArray(const BonValue* bv);
double				BonAsNumber(const BonValue* bv);
const char*			BonAsString(const BonValue* bv);
BonBool				BonAsBool(const BonValue* bv);

const double*			BonAsNumberArray(const BonArray* ba);

BonName				BonCreateName(const char* nameString, size_t nameStringByteCount);

