#pragma once

#include "Bon.h"

#include <stdio.h>

#define BON_STATUS_OK			0
#define BON_STATUS_INVALID_JSON_TEXT	1
#define BON_STATUS_JSON_PARSE_ERROR	2
#define BON_STATUS_JSON_NOT_UTF8	3
#define BON_STATUS_OUT_OF_MEMORY	4
#define BON_STATUS_INVALID_NUMBER	5

/* Must return a memory block with 8 byte alignment */
typedef void*			(*BonTempMemoryAlloc)(void* userdata, size_t byteCount);
typedef void			(*BonTempMemoryFree)(void* userdata, void* ptr);

struct BonParsedJson;

/* Return NULL if first allocation from tempAllocator fails or if tempAllocator is NULL. 
 * Return a valid object otherwise that may or may not have failed.
 * Check for failure with BonGetParsedJsonStatus.
 * If a valid object is returned, then the memory allocated from tempAllocator must be cleaned up by the caller.
 */
struct BonParsedJson*		BonParseJson(BonTempMemoryAlloc tempAlloc, void* tempAllocUserdata, const char* jsonString, size_t jsonStringByteCount);

/* Free all memory allocated for parsedJson including parsedJson itself.
 * Only needed when, for example, using a heap allocator as tempAllocator in BonParseJson
 */
void				BonFreeParsedJsonMemory(struct BonParsedJson* parsedJson, BonTempMemoryFree tempFree, void* tempFreeUserdata);

int				BonGetParsedJsonStatus(struct BonParsedJson* parsedJson);
size_t				BonGetBonRecordSize(struct BonParsedJson* parsedJson);
BonRecord*			BonCreateRecordFromParsedJson(struct BonParsedJson* parsedJson, void* recordMemory);

/* Return a BonRecord allocated with standard malloc. */
BonRecord*			BonCreateRecordFromJson(const char* jsonString, size_t jsonStringByteCount);

void				BonWriteAsJsonToStream(const BonRecord* record, FILE* stream);
