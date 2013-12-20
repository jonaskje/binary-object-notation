#pragma once

#pragma warning(push)
#pragma warning(disable : 4001)
#define _CRTDBG_MAP_ALLOC
#include <stdint.h>
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable : 4820) /* N bytes padding added after data member X */

typedef uint64_t		BonValue;
typedef uint32_t		BonName;
typedef int			BonBool;

typedef struct BonRecord {
	uint16_t		magic;
	uint8_t			version;
	uint8_t			flags;
	BonValue		nameHashObject;		/* A nameString to nameHash lookup dictionary */
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

#define BON_STATUS_OK			0
#define BON_STATUS_INVALID_JSON_TEXT	1
#define BON_STATUS_JSON_PARSE_ERROR	2
#define BON_STATUS_JSON_NOT_UTF8	3
#define BON_STATUS_OUT_OF_MEMORY	4


/* Reading */

BonBool				BonIsAValidRecord(BonRecord* br, size_t sizeBytes);

const char*			BonGetNameString(BonRecord* br, BonName name);
BonValue*			BonGetRootValue(BonRecord* br);

int				BonGetValueType(BonValue* value);

BonObject			BonAsObject(const BonValue* bv);
BonArray			BonAsArray(const BonValue* bv);
double				BonAsNumber(const BonValue* bv);
const char*			BonAsString(const BonValue* bv);
BonBool				BonAsBool(const BonValue* bv);

const double*			BonAsNumberArray(const BonArray* ba);

#pragma warning(pop)