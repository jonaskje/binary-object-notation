#pragma once
/**
* @file
*/

#include "BonFormat.h"

typedef struct BonObject {
	int			count;
	const BonValue*		values;
	const BonName*		names;
} BonObject;

typedef struct BonArray {
	int			count;
	const BonValue*		values;
} BonArray;

BonBool				BonIsAValidRecord(const BonRecord* br);
uint32_t			BonGetRecordSize(const BonRecord* br);

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

