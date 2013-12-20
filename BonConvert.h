#pragma once

#include "Bon.h"

/* Must return a memory block with alignment suitable for storing a pointer */
typedef void* (*BonTempMemoryAllocator)(size_t byteCount);

struct BonParsedJson;

/* Return NULL if first allocation from tempAllocator fails or if tempAllocator is NULL. 
 * Return a valid object otherwise that may or may not have failed.
 * Check for failure with BonGetParsedJsonStatus.
 * If a valid object is returned, then the memory allocated from tempAllocator must be cleaned up by the caller.
 */
struct BonParsedJson*		BonParseJson(BonTempMemoryAllocator tempAllocator, const char* jsonString, size_t jsonStringByteCount);
int				BonGetParsedJsonStatus(struct BonParsedJson* parsedJson);
size_t				BonGetBonRecordSize(struct BonParsedJson* parsedJson);
const BonRecord*		BonCreateRecordFromParsedJson(struct BonParsedJson* parsedJson, void* recordMemory);

/* Return a BonRecord allocated with standard malloc. */
const BonRecord*		BonCreateRecordFromJson(const char* jsonString, size_t jsonStringByteCount);

