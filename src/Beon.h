#pragma once
/* vi: set ts=8 sts=8 sw=8 et: */
/**
* @file
* \addtogroup Beon
* \brief API for editing BON records.
* @{
*/

#include "Bon.h"

struct BeonObject;
struct BeonArray;

struct BeonRecord*              BeonCreateRecord(                       void);

struct BeonArray*               BeonGetRootValueAsArray(                struct BeonRecord* record);
struct BeonObject*              BeonGetRootValueAsObject(               struct BeonRecord* record);

void                            BeonArraySetNull(                       struct BeonArray* array, int index);
void                            BeonArraySetBool(                       struct BeonArray* array, int index, BonBool value);
void                            BeonArraySetNumber(                     struct BeonArray* array, int index, double value);
void                            BeonArraySetString(                     struct BeonArray* array, int index, const char* value);

BonBool                         BeonArrayIsNull(                        struct BeonArray* array, int index);
struct BeonArray*               BeonArrayAsArray(                       struct BeonArray* array, int index);
struct BeonObject*              BeonArrayAsObject(                      struct BeonArray* array, int index);
BonBool                         BeonArrayAsBool(                        struct BeonArray* array, int index);
double                          BeonArrayAsNumber(                      struct BeonArray* array, int index);
const char*                     BeonArrayAsString(                      struct BeonArray* array, int index);


struct BeonValue*               BeonArrayGetElement(                    struct BeonArray* array, int index);


BonBool                         BeonIsNull(                             struct BeonValue* value);
struct BeonArray*               BeonAsArray(                            struct BeonValue* value);
struct BeonObject*              BeonAsObject(                           struct BeonValue* value);
BonBool                         BeonAsBool(                             struct BeonValue* value);
double                          BeonAsNumber(                           struct BeonValue* value);
const char*                     BeonAsString(                           struct BeonValue* value);


struct BonRecord*               BeonFinalizeRecord(                     BonRecord* record);



/** @} */
