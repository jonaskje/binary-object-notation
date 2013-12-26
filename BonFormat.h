#pragma once
/**
* @file
* \addtogroup BonFormat
* @{
*/

#include "Bon.h"

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

