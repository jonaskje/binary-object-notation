#include "Bon.h"
#include <assert.h>
#include <string.h>
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* :Internal */

static void
SetStatusCode(BonContext* ctx, int code) {
	ctx->statusCode = code;
}

static BonBool
IsInFailState(BonContext* ctx) {
	return ctx->statusCode != BON_STATUS_OK;
}

/*---------------------------------------------------------------------------*/
/* :Context */

void				
BonInitContext(BonContext* ctx) {
	ctx->statusCode = BON_STATUS_OK;
}

int				
BonGetAndClearStatus(BonContext* ctx) {
	int s = ctx->statusCode;
	ctx->statusCode = BON_STATUS_OK;
	return s;
}

/*---------------------------------------------------------------------------*/
/* :Reading */

const char*
BonGetNameString(BonContext* ctx, BonRecord* br, BonName name) {
	return 0;
}

/*---------------------------------------------------------------------------*/
/* :Creating */

struct BonObjectEntry;
struct BonArrayEntry;

typedef struct BonStringEntry {
	size_t			byteCount;
	uint8_t			utf8[1];
} BonStringEntry;

typedef struct BonVariant {
	union {
		double			numberValue;
		struct BonObjectEntry*	objectValue;
		struct BonArrayEntry*	arrayValue;
		BonBool			boolValue;
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
	BonContext*		ctx;

	/* Parse context */
	const uint8_t*		cursor;

	/* Results */
	size_t			bonRecordSize;
	BonVariant		rootValue;
} BonParsedJson;

#define BonTempCalloc(tempAllocator, type) ((type*)memset(tempAllocator(sizeof(type)), 0, sizeof(type)))

static BonObjectEntry*
CreateObjectEntry(BonParsedJson* pj) {
	BonObjectEntry* obj = BonTempCalloc(pj->alloc, BonObjectEntry);
	obj->next = obj;
	obj->prev = obj;
	return obj;
}

static BonArrayEntry*
CreateArrayEntry(BonParsedJson* pj) {
	BonArrayEntry* obj = BonTempCalloc(pj->alloc, BonArrayEntry);
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

static BonBool
IsUnexpectedEof(BonParsedJson* pj) {
	if (pj->cursor == pj->jsonStringEnd) {
		SetStatusCode(pj->ctx, BON_STATUS_JSON_PARSE_ERROR);
		return BON_TRUE;
	}
	return BON_FALSE;
}

static BonBool
ParseExpectedChar(BonParsedJson* pj, uint8_t c) {
	assert(pj->cursor != pj->jsonStringEnd);
	if (*pj->cursor++ != c) {
		SetStatusCode(pj->ctx, BON_STATUS_JSON_PARSE_ERROR);
		return BON_FALSE;
	}
	return BON_TRUE;
}

static BonBool
PeekChar(BonParsedJson* pj, uint8_t c) {
	if (IsUnexpectedEof(pj)) {
		return BON_FALSE;
	}
	return *pj->cursor == c ? BON_TRUE : BON_FALSE;
}

static BonStringEntry*
ParseString(BonParsedJson* pj) {
	BonStringEntry* stringEntry = 0;
	const uint8_t* string = 0;
	const uint8_t* stringEnd = 0; 
	uint8_t* dstString;
	size_t bufferSize;
	size_t stringBufferSize;
	if (!ParseExpectedChar(pj, '\"'))
		goto returnEmptyString;
	string = pj->cursor;

	/* Scan for the end of the string */
	while (!IsUnexpectedEof(pj)) {
		if (pj->cursor[0] == '\"' && pj->cursor[-1] != '\\') {
			stringEnd = pj->cursor++;
			break;
		}
		++pj->cursor;
	}

	if (IsInFailState(pj->ctx))
		goto returnEmptyString;

	stringBufferSize =
		+ (stringEnd - string)		/* Min length of string in bytes */
		+ 1;				/* Space for a terminating null */

	bufferSize = offsetof(BonStringEntry, utf8) + stringBufferSize;

	/* So now we know the minimum space we need to parse the string. Allocate it and also make sure there is space for a terminating null */
	stringEntry = (BonStringEntry*)(pj->alloc)(bufferSize);
	stringEntry->byteCount = 0;

	dstString = stringEntry->utf8;

	/* TODO: UTF-8. Just ASCII for now */
	while (string != stringEnd) {
		uint8_t c = *string++;
		
		if (c == 0x5Cu) {
			uint8_t c2;
			if (string == stringEnd) {
				SetStatusCode(pj->ctx, BON_STATUS_JSON_PARSE_ERROR);
				goto clearAndReturnString;
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
		} else if (c >= 0x20u) {
			*dstString++ = c;
		} else {
			SetStatusCode(pj->ctx, BON_STATUS_JSON_PARSE_ERROR);
			goto clearAndReturnString;
		}
	}

	*dstString = 0;
	stringEntry->byteCount = dstString - stringEntry->utf8;
	assert(stringEntry->byteCount < stringBufferSize); 
	return stringEntry;

clearAndReturnString:
	stringEntry->byteCount = 0;
	stringEntry->utf8[0] = 0;
	return stringEntry;

returnEmptyString:
	return BonTempCalloc(pj->alloc, BonStringEntry);
}

static BonVariant s_nullVariant;

static BonVariant
ParseFalseValue(BonParsedJson* pj) {
	BonVariant v;
	if (pj->cursor + 5 > pj->jsonStringEnd || 0 != memcmp("false", pj->cursor, 5)) {
		SetStatusCode(pj->ctx, BON_STATUS_JSON_PARSE_ERROR);
		return s_nullVariant;
	}
	pj->cursor += 5;
	v.type = BON_VT_BOOL;
	v.value.boolValue = BON_FALSE;
	return v;
}

static BonVariant
ParseTrueValue(BonParsedJson* pj) {
	BonVariant v;
	if (pj->cursor + 4 > pj->jsonStringEnd || 0 != memcmp("true", pj->cursor, 4)) {
		SetStatusCode(pj->ctx, BON_STATUS_JSON_PARSE_ERROR);
		return s_nullVariant;
	}
	pj->cursor += 4;
	v.type = BON_VT_BOOL;
	v.value.boolValue = BON_TRUE;
	return v;
}

static BonVariant
ParseNullValue(BonParsedJson* pj) {
	if (pj->cursor + 4 > pj->jsonStringEnd || 0 != memcmp("null", pj->cursor, 4)) {
		SetStatusCode(pj->ctx, BON_STATUS_JSON_PARSE_ERROR);
	}
	pj->cursor += 4;
	return s_nullVariant;
}


static BonVariant
ParseObjectValue(BonParsedJson* pj) {
	assert(0);
	return s_nullVariant;
}

static BonVariant
ParseArrayValue(BonParsedJson* pj) {
	assert(0);
	return s_nullVariant;
}

static BonVariant
ParseStringValue(BonParsedJson* pj) {
	assert(0);
	return s_nullVariant;
}

static BonVariant
ParseNumberValue(BonParsedJson* pj) {
	assert(0);
	return s_nullVariant;
}

static BonVariant
ParseValue(BonParsedJson* pj) {
	if (IsUnexpectedEof(pj)) {
		return s_nullVariant;
	}

	switch (*pj->cursor) {
	case 'f':	return ParseFalseValue(pj);
	case 't':	return ParseTrueValue(pj);
	case 'n':	return ParseNullValue(pj);
	case '{':	return ParseObjectValue(pj);
	case '[':	return ParseArrayValue(pj);
	case '\"':	return ParseStringValue(pj);
	default:	return ParseNumberValue(pj);
	}
}

#define BonAppendToList(head, obj) do { \
		(obj)->next = (head); \
		(obj)->prev = (head)->prev; \
		(head)->prev->next = (obj); \
		(head)->prev = (obj); \
	} while(0)
	

static void
AppendObjectMember(BonParsedJson* pj, BonObjectEntry* objectHead, BonStringEntry* name, BonVariant value) {
	BonObjectEntry* member = CreateObjectEntry(pj);
	member->name = name;
	member->value = value;
	BonAppendToList(objectHead, member);
}

static void
ParseObject(BonParsedJson* pj, BonObjectEntry* object) {
	BonStringEntry*		name;
	BonVariant		value;
	if (IsUnexpectedEof(pj)) {
		return;
	}
	if (!ParseExpectedChar(pj, '{')) {
		return;
	}
	SkipWhitespace(pj);
	if (PeekChar(pj, '}')) {
		goto done;
	}
	while (!IsInFailState(pj->ctx)) {
		name = ParseString(pj);
		SkipWhitespace(pj);
		ParseExpectedChar(pj, ':');
		SkipWhitespace(pj);
		value = ParseValue(pj);
		AppendObjectMember(pj, object, name, value);
		SkipWhitespace(pj);
		if (!PeekChar(pj, ','))
			break;
		ParseExpectedChar(pj, ',');
		SkipWhitespace(pj);
	}
done:
	if (!ParseExpectedChar(pj, '}')) {
		return;
	}
}

static void
ParseArray(BonParsedJson* pj, BonArrayEntry* array) {
}

static void
ParseObjectOrArray(BonParsedJson* pj) {
	if (IsUnexpectedEof(pj))
		return;

	switch(*pj->cursor) {
	case '{':	
		ParseObject(pj, InitObjectVariant(pj, &pj->rootValue));
		break;
	case '[':	
		ParseArray(pj, InitArrayVariant(pj, &pj->rootValue));
		break;
	default:	
		SetStatusCode(pj->ctx, BON_STATUS_JSON_PARSE_ERROR);	
		break;
	}
}

BonParsedJson*
BonParseJson(BonContext* ctx, BonTempMemoryAllocator tempAllocator, const char* jsonString, size_t jsonStringByteCount) {
	BonParsedJson*		pj;
	
	assert(ctx);
	assert(tempAllocator);
	
	pj = BonTempCalloc(tempAllocator, BonParsedJson);
	pj->alloc		= tempAllocator;
	pj->jsonString		= (const uint8_t*)jsonString;
	pj->jsonStringEnd	= (const uint8_t*)jsonString + jsonStringByteCount;
	pj->ctx			= ctx;
	pj->cursor		= (const uint8_t*)jsonString;
	
	if (!jsonString || jsonStringByteCount < 2) {
		SetStatusCode(ctx, BON_STATUS_INVALID_JSON_TEXT);
		goto done;
	}

	/* http://www.ietf.org/rfc/rfc4627.txt, section 3. Encoding
	 * http://en.wikipedia.org/wiki/Byte_order_mark, 
	 * Verify that string is UTF8 by checking for nulls in the first two bytes and also for non UTF8 BOMs*/
	if (jsonString[0] == 0 || jsonString[1] == 0 || jsonString[0] == 0xFEu || jsonString[0] == 0xFFu) {
		SetStatusCode(ctx, BON_STATUS_JSON_NOT_UTF8);
		goto done;
	}

	/* Skip three byte UTF-8 BOM if there is one */
	if (jsonString[0] == 0xEFu) {
		if (jsonStringByteCount < 3 || jsonString[1] != 0xBBu || jsonString[2] != 0xBFu) {
			SetStatusCode(ctx, BON_STATUS_INVALID_JSON_TEXT);
			goto done;
		}

		pj->cursor += 3; /* Skip parsing BOM */
	}

	SkipWhitespace(pj);
	ParseObjectOrArray(pj);
	SkipWhitespace(pj);

	if (pj->cursor != pj->jsonStringEnd) {
		SetStatusCode(ctx, BON_STATUS_JSON_PARSE_ERROR);
		goto done;
	}
done:
	return pj;
}

size_t				
BonGetBonRecordSize(BonContext* ctx, struct BonParsedJson* parsedJson) {
	assert(ctx);
	return parsedJson->bonRecordSize;
}

const BonRecord*		
BonCreateRecordFromParsedJson(BonContext* ctx, struct BonParsedJson* parsedJson, void* recordMemory) {
	assert(ctx);
	return 0;
}

const BonRecord*		
BonCreateRecordFromJson(BonContext* ctx, const char* jsonString, size_t jsonStringByteCount) {
	BonParsedJson*		parsedJson	= BonParseJson(ctx, malloc, jsonString, jsonStringByteCount);
	const BonRecord*	bonRecord	= BonCreateRecordFromParsedJson(ctx, parsedJson, malloc(BonGetBonRecordSize(ctx, parsedJson)));

	assert(0);
	/* Free all memory allocated for parsedJson */
	return bonRecord;
}

/*---------------------------------------------------------------------------*/
/* :Testing */

char s0[] = "   {  \"apa\" : false   } ";

int 
main(int argc, char** argv) {
	BonContext ctx[1];
	BonParsedJson* parsedJson;
	
	BonInitContext(ctx);
	parsedJson = BonParseJson(ctx, malloc, s0, sizeof(s0) - 1);
	assert(BonGetAndClearStatus(ctx) == BON_STATUS_OK);

	return 0;
}
