#include "BonConvert.h"

#pragma warning(push)
#pragma warning(disable : 4001)	/* Single line comments */
#pragma warning(disable : 4820) /* N bytes padding added after data member X */
#include <stdlib.h>
#include <crtdbg.h>
#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable : 4127)	/* Conditional expression is constant */
#pragma warning(disable : 4820) /* N bytes padding added after data member X */
#pragma warning(disable : 4100) /* Unreferenced formal parameter */

struct BonObjectEntry;
struct BonArrayEntry;

typedef struct BonStringEntry {
	size_t			byteCount;
	uint8_t			utf8[1];
} BonStringEntry;

typedef struct BonVariant {
	union {
		BonBool			boolValue;
		double			numberValue;
		struct BonObjectEntry*	objectValue;
		struct BonArrayEntry*	arrayValue;
		BonStringEntry*		stringValue;
	} value;
	int			type;
} BonVariant;

typedef struct BonArrayEntry {
	struct BonArrayEntry*	next;
	struct BonArrayEntry*	prev;
	BonVariant		value;
} BonArrayEntry;

typedef struct BonObjectEntry {
	struct BonObjectEntry*	next;
	struct BonObjectEntry*	prev;
	BonVariant		value;
	BonStringEntry*		name;
} BonObjectEntry;

typedef struct BonParsedJson {
	/* Setup */
	BonTempMemoryAllocator	alloc;
	const uint8_t*		jsonString;
	const uint8_t*		jsonStringEnd;

	/* Parse context */
	const uint8_t*		cursor;
	jmp_buf*		env;

	/* Results */
	int			status;
	size_t			bonRecordSize;
	BonVariant		rootValue;
} BonParsedJson;

static void
GiveUp(jmp_buf* env, int status) {
	longjmp(*env, status);
}

static void*
DoTempCalloc(BonTempMemoryAllocator alloc, jmp_buf* exceptionEnv, size_t size) {
	void* result = alloc(size);
	if (!result) {
		GiveUp(exceptionEnv, BON_STATUS_OUT_OF_MEMORY);
	}
	return memset(result, 0, size);
}

#define BonTempCalloc(tempAllocator, exceptionEnv, type) ((type*)DoTempCalloc(tempAllocator, exceptionEnv, sizeof(type)))

static BonObjectEntry*
CreateObjectEntry(BonParsedJson* pj) {
	BonObjectEntry* obj = BonTempCalloc(pj->alloc, pj->env, BonObjectEntry);
	obj->next = obj;
	obj->prev = obj;
	return obj;
}

static BonArrayEntry*
CreateArrayEntry(BonParsedJson* pj) {
	BonArrayEntry* obj = BonTempCalloc(pj->alloc, pj->env, BonArrayEntry);
	obj->next = obj;
	obj->prev = obj;
	return obj;
}

static BonObjectEntry*
InitObjectVariant(BonParsedJson* pj, BonVariant* v) {
	v->type = BON_VT_OBJECT;
	return v->value.objectValue = CreateObjectEntry(pj);
}

static BonArrayEntry*
InitArrayVariant(BonParsedJson* pj, BonVariant* v) {
	v->type = BON_VT_ARRAY;
	return v->value.arrayValue = CreateArrayEntry(pj);
}

static void
SkipWhitespace(BonParsedJson* pj) {
	for (;;) {
		if (pj->cursor == pj->jsonStringEnd)
			return;

		/* JSON insignificant whitespace */
		switch (*pj->cursor) {
		case 0x20u:
		case 0x09u:
		case 0x0Au:
		case 0x0Du:
			++pj->cursor;
			break;
		default:
			return;
		}
	}
}

static void
FailIfEof(BonParsedJson* pj) {
	if (pj->cursor == pj->jsonStringEnd) {
		GiveUp(pj->env, BON_STATUS_JSON_PARSE_ERROR);
	}
}

static void
FailUnlessCharIs(BonParsedJson* pj, uint8_t c) {
	assert(pj->cursor != pj->jsonStringEnd);
	if (*pj->cursor++ != c) {
		GiveUp(pj->env, BON_STATUS_JSON_PARSE_ERROR);
	}
}

static BonBool
PeekChar(BonParsedJson* pj, uint8_t c) {
	FailIfEof(pj);
	return *pj->cursor == c ? BON_TRUE : BON_FALSE;
}

static void
ParseString(BonParsedJson* pj, BonStringEntry** pstringEntry) {
	BonStringEntry* stringEntry = 0;
	const uint8_t* string = 0;
	const uint8_t* stringEnd = 0;
	uint8_t* dstString;
	size_t bufferSize;
	size_t stringBufferSize;
	FailUnlessCharIs(pj, '\"');
	string = pj->cursor;

	/* Scan for the end of the string */
	for (;;) {
		FailIfEof(pj);
		if (pj->cursor[0] == '\"' && pj->cursor[-1] != '\\') {
			stringEnd = pj->cursor++;
			break;
		}
		++pj->cursor;
	}

	stringBufferSize =
		+(stringEnd - string)		/* Min length of string in bytes */
		+ 1;				/* Space for a terminating null */

	bufferSize = offsetof(BonStringEntry, utf8) + stringBufferSize;

	/* So now we know the minimum space we need to parse the string. Allocate it and also make sure there is space for a terminating null */
	stringEntry = (BonStringEntry*)(pj->alloc)(bufferSize);
	stringEntry->byteCount = 0;

	/* Immediately link the new string entry to the parent to avoid orphaning if the parsing below fails. */
	*pstringEntry = stringEntry;

	dstString = stringEntry->utf8;

	/* TODO: UTF-8. Just ASCII for now */
	while (string != stringEnd) {
		uint8_t c = *string++;

		if (c == 0x5Cu) {
			uint8_t c2;
			if (string == stringEnd) {
				GiveUp(pj->env, BON_STATUS_JSON_PARSE_ERROR);
			}

			c2 = *string++;
			switch (c2) {
			case 0x22u: /* \" */
			case 0x5Cu: /* \\ */
			case 0x2Fu: /* \/ */
				*dstString++ = c2;
				break;
			case 0x62u: /* \b */
				*dstString++ = +0x08u;
				break;
			case 0x66u: /* \f */
				*dstString++ = +0x0Cu;
				break;
			case 0x6Eu: /* \n */
				*dstString++ = +0x0Au;
				break;
			case 0x72u: /* \r */
				*dstString++ = +0x0Du;
				break;
			case 0x74u: /* \t */
				*dstString++ = +0x09u;
				break;
			case 0x75u: /* \u */
				assert(0); /* TODO: Unicode escape */
				break;
			}
		}
		else if (c >= 0x20u) {
			*dstString++ = c;
		}
		else {
			GiveUp(pj->env, BON_STATUS_JSON_PARSE_ERROR);
		}
	}

	*dstString = 0;
	stringEntry->byteCount = dstString - stringEntry->utf8;
	assert(stringEntry->byteCount < stringBufferSize);
}

#define BonAppendToList(head, obj) do { \
		(obj)->next = (head); \
		(obj)->prev = (head)->prev; \
		(head)->prev->next = (obj); \
		(head)->prev = (obj); \
	} while(0)
	
static void			ParseValue(BonParsedJson* pj, BonVariant* value);
static void			ParseObject(BonParsedJson* pj, BonObjectEntry* object);
static void			ParseArray(BonParsedJson* pj, BonArrayEntry* array);

static BonVariant		s_nullVariant		= { BON_VT_NULL, 0 };
static BonVariant		s_boolFalseVariant	= { BON_VT_BOOL, BON_FALSE };
static BonVariant		s_boolTrueVariant	= { BON_VT_BOOL, BON_TRUE };

static BonObjectEntry*
AppendObjectMember(BonParsedJson* pj, BonObjectEntry* objectHead) {
	BonObjectEntry* member = CreateObjectEntry(pj);
	member->name = 0;
	member->value = s_nullVariant;
	BonAppendToList(objectHead, member);
	return member;
}

static void
ParseObject(BonParsedJson* pj, BonObjectEntry* object) {
	FailIfEof(pj);
	FailUnlessCharIs(pj, '{');
	SkipWhitespace(pj);
	if (PeekChar(pj, '}')) {
		goto done;
	}
	for(;;) {
		BonObjectEntry* member = AppendObjectMember(pj, object);
		ParseString(pj, &member->name);
		SkipWhitespace(pj);
		FailUnlessCharIs(pj, ':');
		SkipWhitespace(pj);
		ParseValue(pj, &member->value);
		SkipWhitespace(pj);
		if (!PeekChar(pj, ','))
			break;
		FailUnlessCharIs(pj, ',');
		SkipWhitespace(pj);
	}
done:
	FailUnlessCharIs(pj, '}');
}

static BonArrayEntry*
AppendArrayMember(BonParsedJson* pj, BonArrayEntry* objectHead) {
	BonArrayEntry* member = CreateArrayEntry(pj);
	member->value = s_nullVariant;
	BonAppendToList(objectHead, member);
	return member;
}

static void
ParseArray(BonParsedJson* pj, BonArrayEntry* array) {
	FailIfEof(pj);
	FailUnlessCharIs(pj, '[');
	SkipWhitespace(pj);
	if (PeekChar(pj, ']')) {
		goto done;
	}
	for (;;) {
		BonArrayEntry* member = AppendArrayMember(pj, array);
		ParseValue(pj, &member->value);
		SkipWhitespace(pj);
		if (!PeekChar(pj, ','))
			break;
		FailUnlessCharIs(pj, ',');
		SkipWhitespace(pj);
	}
done:
	FailUnlessCharIs(pj, ']');
}

static void
ParseFalseValue(BonParsedJson* pj, BonVariant* value) {
	if (pj->cursor + 5 > pj->jsonStringEnd || 0 != memcmp("false", pj->cursor, 5)) {
		GiveUp(pj->env, BON_STATUS_JSON_PARSE_ERROR);
	}
	pj->cursor += 5;
	*value = s_boolFalseVariant;
}

static void
ParseTrueValue(BonParsedJson* pj, BonVariant* value) {
	if (pj->cursor + 4 > pj->jsonStringEnd || 0 != memcmp("true", pj->cursor, 4)) {
		GiveUp(pj->env, BON_STATUS_JSON_PARSE_ERROR);
	}
	pj->cursor += 4;
	*value = s_boolTrueVariant;
}

static void
ParseNullValue(BonParsedJson* pj, BonVariant* value) {
	if (pj->cursor + 4 > pj->jsonStringEnd || 0 != memcmp("null", pj->cursor, 4)) {
		GiveUp(pj->env, BON_STATUS_JSON_PARSE_ERROR);
	}
	pj->cursor += 4;
	*value = s_nullVariant;
}

static void
ParseObjectValue(BonParsedJson* pj, BonVariant* value) {
	value->type = BON_VT_OBJECT;
	value->value.objectValue = 0;
	ParseObject(pj, InitObjectVariant(pj, value));
}

static void
ParseArrayValue(BonParsedJson* pj, BonVariant* value) {
	value->type = BON_VT_ARRAY;
	value->value.arrayValue = 0;
	ParseArray(pj, InitArrayVariant(pj, value));
}

static void
ParseStringValue(BonParsedJson* pj, BonVariant* value) {
	value->type = BON_VT_STRING;
	value->value.stringValue = 0;
	ParseString(pj, &value->value.stringValue);
}

static void
ParseNumberValue(BonParsedJson* pj, BonVariant* value) {
	assert(0);
	*value = s_nullVariant;
}

static void
ParseValue(BonParsedJson* pj, BonVariant* value) {
	FailIfEof(pj);
	switch (*pj->cursor) {
	case 'f':	ParseFalseValue(pj, value);	break;
	case 't':	ParseTrueValue(pj, value);	break;
	case 'n':	ParseNullValue(pj, value);	break;
	case '{':	ParseObjectValue(pj, value);	break;
	case '[':	ParseArrayValue(pj, value);	break;
	case '\"':	ParseStringValue(pj, value);	break;
	default:	ParseNumberValue(pj, value);	break;
	}
}

static void
ParseObjectOrArray(BonParsedJson* pj) {
	FailIfEof(pj);

	switch(*pj->cursor) {
	case '{':	
		ParseObject(pj, InitObjectVariant(pj, &pj->rootValue));
		break;
	case '[':	
		ParseArray(pj, InitArrayVariant(pj, &pj->rootValue));
		break;
	default:	
		GiveUp(pj->env, BON_STATUS_JSON_PARSE_ERROR);	
		break;
	}
}

BonParsedJson*
BonParseJson(BonTempMemoryAllocator tempAllocator, const char* jsonString, size_t jsonStringByteCount) {
	BonParsedJson*		pj = 0;
	jmp_buf			errorJmpBuf;
	int			status;

	if (!tempAllocator)
		return 0;

	status = setjmp(errorJmpBuf);

	if (0 == status) {
		pj = BonTempCalloc(tempAllocator, &errorJmpBuf, BonParsedJson);
		if (!pj) {
			GiveUp(&errorJmpBuf, BON_STATUS_OUT_OF_MEMORY);
		}
		pj->env			= &errorJmpBuf;
		pj->alloc		= tempAllocator;
		pj->jsonString		= (const uint8_t*)jsonString;
		pj->jsonStringEnd	= (const uint8_t*)jsonString + jsonStringByteCount;
		pj->cursor		= (const uint8_t*)jsonString;

		if (!jsonString || jsonStringByteCount < 2) {
			GiveUp(pj->env, BON_STATUS_INVALID_JSON_TEXT);
		}

		/* http://www.ietf.org/rfc/rfc4627.txt, section 3. Encoding
		 * http://en.wikipedia.org/wiki/Byte_order_mark, 
		 * Verify that string is UTF8 by checking for nulls in the first two bytes and also for non UTF8 BOMs*/
		if (jsonString[0] == 0 || jsonString[1] == 0 || jsonString[0] == 0xFEu || jsonString[0] == 0xFFu) {
			GiveUp(pj->env, BON_STATUS_JSON_NOT_UTF8);
		}

		/* Skip three byte UTF-8 BOM if there is one */
		if (jsonString[0] == 0xEFu) {
			if (jsonStringByteCount < 3 || jsonString[1] != 0xBBu || jsonString[2] != 0xBFu) {
				GiveUp(pj->env, BON_STATUS_INVALID_JSON_TEXT);
			}

			pj->cursor += 3; /* Skip parsing BOM */
		}

		SkipWhitespace(pj);
		ParseObjectOrArray(pj);
		SkipWhitespace(pj);

		if (pj->cursor != pj->jsonStringEnd) {
			GiveUp(pj->env, BON_STATUS_JSON_PARSE_ERROR);
		}
	} else {
		if (pj) {
			pj->status = status;
		}
	}
	return pj;
}

int				
BonGetParsedJsonStatus(struct BonParsedJson* parsedJson) {
	if (parsedJson) {
		return parsedJson->status;
	} else {
		return BON_STATUS_OUT_OF_MEMORY;
	}
}

size_t				
BonGetBonRecordSize(struct BonParsedJson* parsedJson) {
	assert(parsedJson->status == BON_STATUS_OK);
	return parsedJson->bonRecordSize;
}

const BonRecord*		
BonCreateRecordFromParsedJson(struct BonParsedJson* parsedJson, void* recordMemory) {
	assert(parsedJson->status == BON_STATUS_OK);
	return 0;
}

static void
FreeString(BonStringEntry* v) {
	if (!v)
		return;
	free(v);
}

static void 
FreeVariant(BonVariant* v) {
	switch (v->type) {
	case BON_VT_OBJECT: {
		BonObjectEntry* head	= v->value.objectValue;
		BonObjectEntry* p;
		if (!head)
			return;
		p = head->next;
		while (p != head) {
			BonObjectEntry* next = p->next;
			FreeString(p->name);
			FreeVariant(&p->value);
			free(p);
			p = next;
		}
		free(head);
		break;
	}
	case BON_VT_ARRAY: {
		BonArrayEntry* head = v->value.arrayValue;
		BonArrayEntry* p;
		if (!head)
			return;
		p = head->next;
		while (p != head) {
			BonArrayEntry* next = p->next;
			FreeVariant(&p->value);
			free(p);
			p = next;
		}
		free(head);
		break;
	}
	case BON_VT_STRING: {
		FreeString(v->value.stringValue);
		break;
	}

	}
}

static void 
FreeParsedJson(BonParsedJson* parsedJson) {
	if (parsedJson) {
		FreeVariant(&parsedJson->rootValue);
		free(parsedJson);
	}
}

const BonRecord*		
BonCreateRecordFromJson(const char* jsonString, size_t jsonStringByteCount) {
	BonParsedJson*		parsedJson	= BonParseJson(malloc, jsonString, jsonStringByteCount);
	const BonRecord*	bonRecord	= BonCreateRecordFromParsedJson(parsedJson, malloc(BonGetBonRecordSize(parsedJson)));

	FreeParsedJson(parsedJson);

	assert(0);
	return bonRecord;
}

/*---------------------------------------------------------------------------*/
/* :Testing */

/* Tests starting with + are expected to succeed and those that start with - are expected to fail */
static const char s_tests[] =
	"+   {  \"apa\" : false, \"Skill\":null }  \0"
	"+[]\0"
	"+[false,true,null,false,\"apa\",{},[]]\0"
	"+[false,true,null,false,\"apa\",{\"foo\":false},[\"a\",false,null]]\0"
	"-[\0"
	"-\0"
	"-[ \"abc ]\0"
	"-[false,true,null,false,\"apa\",{\"foo\":false},[\"a\",false,nul]]\0"
	"\0"; /* Terminate tests */

int 
main(int argc, char** argv) {
	const char* test = s_tests;
	while(*test) {
		int expectedResult		= (*test++ == '+') ? BON_TRUE : BON_FALSE;
		size_t len			= strlen(test);
		BonParsedJson* parsedJson	= BonParseJson(malloc, test, len);
		if (!parsedJson) {
			if (expectedResult == BON_TRUE) {
				printf("FAIL (-): %s\n", test);
			}
		} else {
			if ((parsedJson->status == BON_STATUS_OK) != expectedResult) {
				printf("FAIL (%d): %s\n", parsedJson->status, test);
			}
		}
		FreeParsedJson(parsedJson);
		test += len + 1;
	}

	_CrtDumpMemoryLeaks();
	return 0;
}

#pragma warning(pop)
