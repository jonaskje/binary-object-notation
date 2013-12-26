#pragma once
/* vi: set ts=8 sts=8 sw=8 noet: */
/**
* @file
* \addtogroup Bon
* @{
*/

#include <stdint.h>
#include <stdlib.h>

typedef uint64_t		BonValue;
typedef uint32_t		BonName;
typedef int32_t			BonBool;

#define BON_VT_NUMBER		0
#define BON_VT_BOOL		1
#define BON_VT_STRING		2
#define BON_VT_ARRAY		3
#define BON_VT_OBJECT		4
#define BON_VT_NULL		7

#define BON_FALSE		0
#define BON_TRUE		1

typedef struct BonObject {
	int			count;
	const BonValue*		values;
	const BonName*		names;
} BonObject;

typedef struct BonArray {
	int			count;
	const BonValue*		values;
} BonArray;

/**
 * A BON record header.
 */
typedef struct BonRecord {
	uint32_t		magic;			/**< FourCC('B', 'O', 'N', ' ') */
	uint32_t		recordSize;		/**< Total size of the entire record */
	uint32_t		reserved;		/**< Must be 0 */
	uint32_t		reserved1;		/**< Must be 0 */
	int32_t			valueStringOffset;	/**< Offset to first value string &valueStringOffset */
	int32_t			nameLookupTableOffset;	/**< Offset to name lookup table relative &nameLookupTableOffset */
	BonValue		rootValue;		/**< Variant referencing the root value in the record (an array or an object) */
} BonRecord;

BonBool				BonIsAValidRecord(const BonRecord* br, size_t brSizeInBytes);
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

/** @} */

/**
* @file
* \addtogroup BonFormat
* @{
*/

/**
 * A BonValue when type is BON_VT_ARRAY
 */
typedef struct BonArrayValue {
	int32_t			type;			/**< BON_VT_ARRAY **/
	int32_t			offset;			/**< Address of this struct + offset points to the array */
} BonArrayValue;

/**
 * A BonValue when type is BON_VT_OBJECT
 */
typedef struct BonObjectValue {
	int32_t			type;			/**< BON_VT_OBJECT **/
	int32_t			offset;			/**< Address of this struct + offset points to the object */
} BonObjectValue;

/**
 * A BonValue when type is BON_VT_STRING
 */
typedef struct BonStringValue {
	int32_t			type;			/**< BON_VT_STRING **/
	int32_t			offset;			/**< Address of this struct + offset points to the string */
} BonStringValue;

/**
 * A BonValue when type is BON_VT_BOOL
 */
typedef struct BonBoolValue {
	int32_t			type;			/**< BON_VT_BOOL **/
	BonBool			value;			/**< BON_FALSE or BON_TRUE */
} BonBoolValue;

/**
 * A header for an object or an array.
 * To differentiate between arrays and objects, objects have a negative capacity and arrays a
 * positive capacity. A stored BON record always have abs(capacity)==abs(count).
 */
typedef struct BonContainerHeader {
	int32_t			capacity;		/**< Container capacity */
	int32_t			count;			/**< Number of items in the container */
} BonContainerHeader;

/**
 * An entry in the name lookup table.
 */
typedef struct BonNameAndOffset {
	BonName			name;			/**< The hash of a object member's name. */
	int32_t			offset;			/**< Address of offset + offset points to the name's string. */
} BonNameAndOffset;

/** @} */
