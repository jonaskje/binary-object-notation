#pragma once

#include <stdlib.h>
#include <stdint.h>

typedef uint64_t		BonValue;
typedef uint32_t		BonName;
typedef int			BonBool;

typedef struct BonContext {
	int			statusCode;
} BonContext;

typedef struct BonRecord {
	uint16_t		magic;
	uint8_t			version;
	uint8_t			flags;
	BonValue		nameHashObject;		// A nameString to nameHash lookup dictionary
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

#define BON_VT_NULL		0
#define BON_VT_BOOL		1
#define BON_VT_NUMBER		2
#define BON_VT_STRING		3
#define BON_VT_ARRAY		4
#define BON_VT_OBJECT		5

#define BON_FALSE		0
#define BON_TRUE		1

#define BON_STATUS_OK			0
#define BON_STATUS_INVALID_JSON_TEXT	1
#define BON_STATUS_JSON_PARSE_ERROR	2
#define BON_STATUS_JSON_NOT_UTF8	3

/* Context */

void				BonInitContext(BonContext* ctx);
int				BonGetAndClearStatus(BonContext* ctx);

/* Reading */

BonBool				BonIsAValidRecord(BonContext* ctx, BonRecord* br, size_t sizeBytes);

const char*			BonGetNameString(BonContext* ctx, BonRecord* br, BonName name);
BonValue*			BonGetRootValue(BonContext* ctx, BonRecord* br);

int				BonGetValueType(BonContext* ctx, BonValue* value);

BonObject			BonAsObject(BonContext* ctx, const BonValue* bv);
BonArray			BonAsArray(BonContext* ctx, const BonValue* bv);
double				BonAsNumber(BonContext* ctx, const BonValue* bv);
const char*			BonAsString(BonContext* ctx, const BonValue* bv);
BonBool				BonAsBool(BonContext* ctx, const BonValue* bv);

const double*			BonAsNumberArray(BonContext* ctx, const BonArray* ba);

/* Creating */

/* Must return a memory block with alignment suitable for storing a pointer */
typedef void* (*BonTempMemoryAllocator)(size_t byteCount);

struct BonParsedJson;

struct BonParsedJson*		BonParseJson(BonContext* ctx, BonTempMemoryAllocator tempAllocator, const char* jsonString, size_t jsonStringByteCount);
size_t				BonGetBonRecordSize(BonContext* ctx, struct BonParsedJson* parsedJson);
const BonRecord*		BonCreateRecordFromParsedJson(BonContext* ctx, struct BonParsedJson* parsedJson, void* recordMemory);

/* Return a BonRecord allocated with standard malloc. */
const BonRecord*		BonCreateRecordFromJson(BonContext* ctx, const char* jsonString, size_t jsonStringByteCount);
