#pragma once
/**
* @file
*/

#include "Bon.h"

#include <stdio.h>

/* Low-level API */

#define				BON_STATUS_OK			0
#define				BON_STATUS_INVALID_JSON_TEXT	1
#define				BON_STATUS_JSON_PARSE_ERROR	2
#define				BON_STATUS_JSON_NOT_UTF8	3
#define				BON_STATUS_OUT_OF_MEMORY	4
#define				BON_STATUS_UNALIGNED_MEMORY	5		/**< Memory returned by allocator must be 8-byte aligned */
#define				BON_STATUS_INVALID_NUMBER	6

/* Must return a memory block with 8 byte alignment */
typedef void*			(*BonTempMemoryAlloc)(		void*				userdata,		
								size_t				byteCount);

typedef void			(*BonTempMemoryFree)(		void*				userdata, 
								void*				ptr);

struct BonParsedJson;

/**
 * \brief Parse a sequence of JSON UTF-8 data into an intermediate format.
 *
 * Parse a sequence of UTF-8 encoded JSON into an intermediate representation. 
 * From the intermediate representation a required size for the BonRecord can extracted using
 * BonGetBonRecordSize. The caller may then allocate an appropriate amount of memory and call
 * BonCreateRecordFromParsedJson which will use the intermediate representation to create
 * the final BonRecord.
 *
 * After the BonRecord has been created the work memory allocated through tempAlloc must be
 * released by the caller. How that is done depends on what tempAlloc does. If tempAlloc
 * is a linear memory allocator, then the caller only need to reset the allocator to what
 * it was before the call to BonParseJson. If on the other hand tempAlloc is a heap allocator,
 * then the caller can use the BonFreeParsedJsonMemory function which will call a callback
 * for each allocated chunk of memory.
 *
 * ~~~
 * void* MyAllocator(size_t size, void* userdata) { 
 *	return ((MyAllocatorType*)userdata)->alloc(size); 
 * }
 *
 * MyAllocatorType myAllocatorInstance;
 * BonRecord* record;
 * char jsonEtc[] = "[1, 2, 3] etc";
 * size_t jsonSize = 9; 
 * struct BonParsedJson* pj = BonParseJson(MyAllocator, &myAllocatorInstance, json, jsonSize);
 * if (!pj || BonGetParsedJsonStatus(pj) != BON_STATUS_OK) { 
 *	fail(); 
 * }
 * record = (BonRecord*)malloc(BonGetBonRecordSize(pj));
 * BonCreateRecordFromParsedJson(pj, record);
 * ~~~
 *
 * @param tempAlloc		A function used to allocate temporary working memory.
 * @param temAllocUserdata	A user provided pointer always passed to tempAlloc (e.g. an instance pointer to an allocator).
 * @param jsonData		A sequence of UTF-8 encoded JSON. Does not need to be NULL-terminated. 
 * @param jsonDataByteCount	Number of bytes to parse from jsonData.
 * @return			Return NULL if first allocation from tempAlloc fails or if tempAlloc is NULL.
 *                              Otherwise return a valid object that may or may not have failed.
 *                              Check for failure with BonGetParsedJsonStatus.
 *                              If a valid object is returned, then the memory allocated from tempAllocator must be cleaned up by the caller.
 */
struct BonParsedJson*		BonParseJson(			BonTempMemoryAlloc		tempAlloc, 
								void*				tempAllocUserdata, 
								const char*			jsonData, 
								size_t				jsonDataByteCount);

/**
 * \brief Free all memory allocated for parsedJson including parsedJson itself.
 *
 * Call tempFree for every chunk of memory allocated by BonParseJson. tempFreeUserdata is passed
 * to each call to tempFree and can for example point to an instance of an allocator.
 * Calling this function is optional but is a help when using a heap allocator.
 *
 * @param parsedJson		The intermediate data which should be deleted.
 * @param tempFree		Callback which will be called to free each allocated chunk of memory. 
 * @param tempFreeUserdata	Userdata passed to each call to tempFree
 */
void				BonFreeParsedJsonMemory(	struct BonParsedJson*		parsedJson, 
								BonTempMemoryFree		tempFree, 
								void*				tempFreeUserdata);

/**
 * \brief Get the result from a call to BonParseJson.
 *
 * The status is one of the BON_STATUS_* values. The only function that is safe to call when the
 * status is != BON_STATUS_OK is BonFreeParsedJsonMemory, which can free a partially parsed JSON
 * document.
 *
 * @param parsedJson		The intermediate data which status should be checked.
 */
int				BonGetParsedJsonStatus(		struct BonParsedJson*		parsedJson);

size_t				BonGetBonRecordSize(		struct BonParsedJson*		parsedJson);

BonRecord*			BonCreateRecordFromParsedJson(	struct BonParsedJson*		parsedJson, 
								void*				recordMemory);


/* High-level API */

/* Return a BonRecord allocated with standard malloc. */
BonRecord*			BonCreateRecordFromJson(	const char*			jsonString, 
								size_t				jsonStringByteCount);
void				BonWriteAsJsonToStream(		const BonRecord*		record, 
								FILE*				stream);


void				BonDebugWrite(			const BonRecord* r, 
								FILE* stream);

