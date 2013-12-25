#pragma once
/**
* @file
*/

#include "Bon.h"

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

typedef struct BonBoolValue {
	int32_t			type;
	int32_t			value;
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

typedef struct BonNameAndOffset {
	BonName			name;
	int32_t			offset;
} BonNameAndOffset;


