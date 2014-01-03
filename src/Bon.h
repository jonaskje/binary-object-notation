#pragma once
/* vi: set ts=8 sts=8 sw=8 et: */
/**
* @file
* \addtogroup Bon
* \brief API for reading BON records.
*
* It is expected that the client has a BonRecord somewhere in memory, loaded from disk 
* for example, and that the client knows the size of the record.
*
* Before accessing the BON record it is good to check that the BonRecord is valid by
* calling BonIsAValidRecord.
* @{
*/

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t                BonValue;
typedef uint32_t                BonName;
typedef int32_t                 BonBool;

#define BON_VT_NUMBER           0
#define BON_VT_BOOL             1
#define BON_VT_STRING           2
#define BON_VT_ARRAY            3
#define BON_VT_OBJECT           4
#define BON_VT_NULL             7

#define BON_FALSE               0
#define BON_TRUE                1

typedef struct BonObject {
        int                     count;
        const BonValue*         values;
        const BonName*          names;
} BonObject;

typedef struct BonArray {
        int                     count;
        const BonValue*         values;
} BonArray;

typedef struct BonNumberArray {
        int                     count;
        const double*           values;
} BonNumberArray;

/**
 * \brief A BON record header.
 */
typedef struct BonRecord {
        uint32_t                magic;                          /**< FourCC('B', 'O', 'N', ' ') */
        uint32_t                recordSize;                     /**< Total size of the entire record */
        uint32_t                reserved;                       /**< Must be 0 */
        uint32_t                reserved1;                      /**< Must be 0 */
        int32_t                 valueStringOffset;              /**< Offset to first value string &valueStringOffset */
        int32_t                 nameLookupTableOffset;          /**< Offset to name lookup table relative &nameLookupTableOffset */
        BonValue                rootValue;                      /**< Variant referencing the root value in the record (an array or an object) */
} BonRecord;

/**
 * @param brSizeInBytes - either zero (don't do any size checks) or equal the size of the BonRecord.
 */
BonBool                         BonIsAValidRecord(              const BonRecord* br, 
                                                                size_t brSizeInBytes);

uint32_t                        BonGetRecordSize(               const BonRecord* br);

const char*                     BonGetNameString(               const BonRecord* br, 
                                                                BonName name);

/** 
 * \brief Return a BON record's root value.
 * A root value is always an array or an object. It can never be null.
 */
const BonValue*                 BonGetRootValue(                const BonRecord* br);

/** 
 * \brief Return the type of a value. 
 * \sa BON_VT_NULL, BON_VT_BOOL, BON_VT_NUMBER, BON_VT_STRING, BON_VT_ARRAY, BON_VT_OBJECT 
 */
int                             BonGetValueType(                const BonValue* value);

/** Return BON_TRUE if a value is BON_VT_NULL. */
BonBool                         BonIsNullValue(                 const BonValue* value);

/** Read a value as a BON_VT_OBJECT. If bv is of another type, then return an empty object. */
BonObject                       BonAsObject(                    const BonValue* bv);

/** 
 * \brief Read a value as a BON_VT_ARRAY. If bv is of another type, then return an empty array. 
 * \sa BonAsNumberArray
 */
BonArray                        BonAsArray(                     const BonValue* bv);

/** 
 * \brief Read a value as an array of numbers. 
 *
 * This is a very efficient way of working on large arrays of numbers.
 *
 * Important! There is no error checking that the array is actually homogenous and only contains
 * numbers. So this function also expects that you know what you are doing.
 *
 * If bv isn't a BON_VT_ARRAY then an empty array is returned.
 * \sa BonAsArray
 */
BonNumberArray                  BonAsNumberArray(               const BonValue* bv);

/** Read a value as a BON_VT_NUMBER. If bv is of another type, then return 0.0 */
double                          BonAsNumber(                    const BonValue* bv);

/** Read a value as a BON_VT_STRING. If bv is of another type, then return an empty string (non-null)*/
const char*                     BonAsString(                    const BonValue* bv);

/** Read a value as a BON_VT_BOOL. If bv is of another type, then return BON_FALSE */
BonBool                         BonAsBool(                      const BonValue* bv);

/** Return the hash of a UTF-8 encoded sequence of bytes */
BonName                         BonCreateName(                  const char* nameString, 
                                                                size_t nameStringByteCount);

/** Return the hash of a null-terminated UTF-8 encoded sequence of bytes */
BonName                         BonCreateNameCstr(              const char* nameString);

/** Return index of value in sorted array names. Return -1 if value is not in the names array. */
int                             BonFindIndexOfName(             const BonName* names,
	                                                        int nameCount,
								BonName value);



int                             BonGetMemberValueType(          const BonObject* object,
                                                                BonName name);

BonBool                         BonIsMemberNullValue(           const BonObject* object,
                                                                BonName name);

BonObject                       BonMemberAsObject(              const BonObject* object,      
                                                                BonName name);

BonArray                        BonMemberAsArray(               const BonObject* object,
                                                                BonName name);

BonNumberArray                  BonMemberAsNumberArray(         const BonObject* object,
                                                                BonName name);

double                          BonMemberAsNumber(              const BonObject* object,
                                                                BonName name);

const char*                     BonMemberAsString(              const BonObject* object,
                                                                BonName name);

BonBool                         BonMemberAsBool(                const BonObject* object,
                                                                BonName name);

/** @} */

/**
* @file
* \addtogroup BonFormat
* \brief Structs and definitions that help when working with raw BON records. The functions in \ref Bon should be used instead of these if possible.
* @{
*/

/**
 * A BonValue when type is BON_VT_ARRAY
 */
typedef struct BonArrayValue {
        int32_t                 type;                   /**< BON_VT_ARRAY **/
        int32_t                 offset;                 /**< Address of this struct + offset points to the array */
} BonArrayValue;

/**
 * A BonValue when type is BON_VT_OBJECT
 */
typedef struct BonObjectValue {
        int32_t                 type;                   /**< BON_VT_OBJECT **/
        int32_t                 offset;                 /**< Address of this struct + offset points to the object */
} BonObjectValue;

/**
 * A BonValue when type is BON_VT_STRING
 */
typedef struct BonStringValue {
        int32_t                 type;                   /**< BON_VT_STRING **/
        int32_t                 offset;                 /**< Address of this struct + offset points to the string */
} BonStringValue;

/**
 * A BonValue when type is BON_VT_BOOL
 */
typedef struct BonBoolValue {
        int32_t                 type;                   /**< BON_VT_BOOL **/
        BonBool                 value;                  /**< BON_FALSE or BON_TRUE */
} BonBoolValue;

/**
 * A header for an object or an array.
 * To differentiate between arrays and objects, objects have a negative capacity and arrays a
 * positive capacity. A stored BON record always have abs(capacity)==abs(count).
 */
typedef struct BonContainerHeader {
        int32_t                 capacity;               /**< Container capacity */
        int32_t                 count;                  /**< Number of items in the container */
} BonContainerHeader;

/**
 * An entry in the name lookup table.
 */
typedef struct BonNameAndOffset {
        BonName                 name;                   /**< The hash of a object member's name. */
        int32_t                 offset;                 /**< Address of offset + offset points to the name's string. */
} BonNameAndOffset;


#ifdef __cplusplus
}
#endif

/** @} */
