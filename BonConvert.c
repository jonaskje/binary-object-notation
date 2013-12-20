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
#include <ctype.h>
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable : 4127)	/* Conditional expression is constant */
#pragma warning(disable : 4820) /* N bytes padding added after data member X */
#pragma warning(disable : 4100) /* Unreferenced formal parameter */

/*---------------------------------------------------------------------------*/
/* List helpers */

#define BonPrependToList(head, obj) \
	do { \
		(obj)->next = *(head); \
		*(head) = obj; \
	} while (0)


/* Mergesort adapted from http://www.chiark.greenend.org.uk/~sgtatham/algorithms/listsort.c
 *
 * This [function] is copyright 2001 Simon Tatham.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL SIMON TATHAM BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/
#define BonSortList(head, ElementType, CompareFun) \
	do {	                                                                               \
		ElementType *p, *q, *e, *tail, *oldhead, *list;				       \
		int insize, nmerges, psize, qsize, i;					       \
		list = *(head);								       \
		if (!list) break;							       \
		insize = 1;								       \
		for (;;) {								       \
			p = oldhead = list;						       \
			list = tail = 0;						       \
			nmerges = 0;							       \
			while (p) {							       \
				nmerges++;						       \
				q = p;							       \
				psize = 0;						       \
				for (i = 0; i < insize; i++) {				       \
					psize++;					       \
					q = q->next;					       \
					if (!q) break;					       \
				}							       \
				qsize = insize;						       \
				while (psize > 0 || (qsize > 0 && q)) {			       \
					if (psize == 0) {				       \
						e = q; q = q->next; qsize--;		       \
					}						       \
					else if (qsize == 0 || !q) {			       \
						e = p; p = p->next; psize--;		       \
					}						       \
					else if (CompareFun(p, q) <= 0) {		       \
						e = p; p = p->next; psize--;		       \
					}						       \
					else {						       \
						e = q; q = q->next; qsize--;		       \
					}						       \
					if (tail) {					       \
						tail->next = e;				       \
					}						       \
					else {						       \
						list = e;				       \
					}						       \
					tail = e;					       \
				}							       \
				p = q;							       \
			}								       \
			tail->next = NULL;						       \
			if (nmerges <= 1) {						       \
				*(head) = list;						       \
				break;							       \
			}								       \
			insize *= 2;                                                           \
		}									       \
	} while(0)

/*---------------------------------------------------------------------------*/
/* Internal */

struct BonObjectEntry;
struct BonArrayEntry;

typedef struct BonStringEntry {
	struct BonStringEntry*	next;
	struct BonStringEntry*	alias;
	size_t			byteCount;
	BonName			hash;
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
	BonVariant		value;
} BonArrayEntry;

typedef struct BonObjectEntry {
	struct BonObjectEntry*	next;
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

	BonStringEntry*		nameStringList;
	BonStringEntry*		valueStringList;
	int			objectCount;
	int			objectMemberCount;
	int			objectMemberNameCount;
	int			arrayCount;
	int			arrayMemberCount;

	/* Results */
	int			status;
	size_t			bonRecordSize;
	BonVariant		rootValue;
} BonParsedJson;

static size_t 
BonRoundUp(size_t value, size_t multiple) {
	if (multiple == 0) {
		return value;
	} else {
		const size_t remainder = value % multiple;
		if (remainder == 0)
			return value;
		return value + multiple - remainder;
	}
}

__declspec(noreturn) static void
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
	return BonTempCalloc(pj->alloc, pj->env, BonObjectEntry);
}

static BonArrayEntry*
CreateArrayEntry(BonParsedJson* pj) {
	return BonTempCalloc(pj->alloc, pj->env, BonArrayEntry);
}

static BonObjectEntry**
InitObjectVariant(BonParsedJson* pj, BonVariant* v) {
	v->type = BON_VT_OBJECT;
	v->value.objectValue = 0;
	return &v->value.objectValue;
}

static BonArrayEntry**
InitArrayVariant(BonParsedJson* pj, BonVariant* v) {
	v->type = BON_VT_ARRAY;
	v->value.arrayValue = 0;
	return &v->value.arrayValue;
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
	BonStringEntry*		stringEntry	= 0;
	const uint8_t*		string		= 0;
	const uint8_t*		stringEnd	= 0;
	uint8_t*		dstString;
	size_t			bufferSize;
	size_t			stringBufferSize;

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
	stringEntry->byteCount	= 0;
	stringEntry->hash	= 0;
	stringEntry->alias	= stringEntry;	/* I.e. no alias */
	stringEntry->next	= 0;

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
	stringEntry->byteCount	= dstString - stringEntry->utf8;
	stringEntry->hash	= BonCreateName((const char*)stringEntry->utf8, stringEntry->byteCount);
	assert(stringEntry->byteCount < stringBufferSize);
}

static void			ParseValue(BonParsedJson* pj, BonVariant* value);
static void			ParseObject(BonParsedJson* pj, BonObjectEntry** objectHead);
static void			ParseArray(BonParsedJson* pj, BonArrayEntry** arrayHead);

static BonVariant		s_nullVariant		= { BON_VT_NULL, 0 };
static BonVariant		s_boolFalseVariant	= { BON_VT_BOOL, BON_FALSE };
static BonVariant		s_boolTrueVariant	= { BON_VT_BOOL, BON_TRUE };

static BonObjectEntry*
AppendObjectMember(BonParsedJson* pj, BonObjectEntry** objectHead) {
	BonObjectEntry* member = CreateObjectEntry(pj);
	member->name = 0;
	member->value = s_nullVariant;
	BonPrependToList(objectHead, member);
	return member;
}

static int
ObjectNameCompare(const BonObjectEntry* a, const BonObjectEntry* b) {
	return (int)((int64_t)(uint64_t)(a->name->hash) - (int64_t)(uint64_t)(b->name->hash));
}

static void
ParseObject(BonParsedJson* pj, BonObjectEntry** objectHead) {
	int memberCount = 0;
	FailIfEof(pj);
	FailUnlessCharIs(pj, '{');
	
	pj->objectCount++;
	
	SkipWhitespace(pj);
	if (PeekChar(pj, '}')) {
		goto done;
	}
	for(;;) {
		BonObjectEntry* member = AppendObjectMember(pj, objectHead);
		memberCount++;
		ParseString(pj, &member->name);
		BonPrependToList(&pj->nameStringList, member->name);
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
	pj->objectMemberCount += memberCount;
	pj->objectMemberNameCount += (int)BonRoundUp(memberCount, 2);	/* Account for round up when odd number of members */

	/* Canonicalize */
	BonSortList(objectHead, BonObjectEntry, ObjectNameCompare);
done:
	FailUnlessCharIs(pj, '}');
}

static BonArrayEntry*
AppendArrayMember(BonParsedJson* pj, BonArrayEntry** arrayHead) {
	BonArrayEntry* member = CreateArrayEntry(pj);
	member->value = s_nullVariant;
	BonPrependToList(arrayHead, member); /* Note: reverse order */
	return member;
}

static void
ParseArray(BonParsedJson* pj, BonArrayEntry** arrayHead) {
	FailIfEof(pj);
	FailUnlessCharIs(pj, '[');

	pj->arrayCount++;

	SkipWhitespace(pj);
	if (PeekChar(pj, ']')) {
		goto done;
	}
	for (;;) {
		BonArrayEntry* member = AppendArrayMember(pj, arrayHead);

		pj->arrayMemberCount++;

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
	BonPrependToList(&pj->valueStringList, value->value.stringValue);
}

/* Convert at most n characters starting from string to a double.
 * pend is the position of first non-double character.
 * If the conversion fails, then 0.0 is returned and *endPtr == string
 * This version does not allow initial whitespace.
 * 
 * Adapted from http://svn.ruby-lang.org/repos/ruby/branches/ruby_1_8/missing/strtod.c
 * 
 * Copyright (c) 1988-1993 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */
static int maxExponent = 511;	/* Largest possible base 10 exponent.  Any
				 * exponent larger than this will already
				 * produce underflow or overflow, so there's
				 * no need to worry about additional digits.
				 */
static double powersOf10[] = {	/* Table giving binary powers of 10.  Entry */
	10.,			/* is 10^2^i.  Used to convert decimal */
	100.,			/* exponents into floating-point numbers. */
	1.0e4,
	1.0e8,
	1.0e16,
	1.0e32,
	1.0e64,
	1.0e128,
	1.0e256
};

static double
StringToDouble(const char* string, size_t n, const char** endPtr) {
	int sign = BON_FALSE, expSign = BON_FALSE;
	double fraction, dblExp, *d;
	register const char *p;
	register int c;
	int exp = 0;		/* Exponent read from "EX" field. */
	int fracExp = 0;	/* Exponent that derives from the fractional
				 * part.  Under normal circumstances, it is
				 * the negative of the number of digits in F.
				 * However, if I is very long, the last digits
				 * of I get dropped (otherwise a long I with a
				 * large negative exponent could cause an
				 * unnecessary overflow on I alone).  In this
				 * case, fracExp is incremented one for each
				 * dropped digit. */
	int mantSize;		/* Number of digits in mantissa. */
	int decPt;		/* Number of mantissa digits BEFORE decimal
				 * point. */
	const char *pExp;	/* Temporarily holds location of exponent
				 * in string. */
	const char *pEnd;	/* Max position of p */

	p = string;
	pEnd = string + n;

	if (p != pEnd) {
		if (*p == '-') {
			sign = BON_TRUE;
			p += 1;
		}
		else {
			if (*p == '+') {
				p += 1;
			}
			sign = BON_FALSE;
		}
	}

	/*
	 * Count the number of digits in the mantissa (including the decimal
	 * point), and also locate the decimal point.
	 */

	decPt = -1;
	for (mantSize = 0; p != pEnd; mantSize += 1)
	{
		c = *p;
		if (!isdigit(c)) {
			if ((c != '.') || (decPt >= 0)) {
				break;
			}
			decPt = mantSize;
		}
		p += 1;
	}

	/*
	 * Now suck up the digits in the mantissa.  Use two integers to
	 * collect 9 digits each (this is faster than using floating-point).
	 * If the mantissa has more than 18 digits, ignore the extras, since
	 * they can't affect the value anyway.
	 */

	pExp = p;
	p -= mantSize;
	if (decPt < 0) {
		decPt = mantSize;
	}
	else {
		mantSize -= 1;			/* One of the digits was the point. */
	}
	if (mantSize > 18) {
		fracExp = decPt - 18;
		mantSize = 18;
	}
	else {
		fracExp = decPt - mantSize;
	}
	if (mantSize == 0) {
		goto fail;
	}
	else {
		int frac1, frac2;
		frac1 = 0;
		for (; mantSize > 9 && p != pEnd; mantSize -= 1)
		{
			c = *p;
			p += 1;
			if (c == '.') {
				c = *p;
				p += 1;
			}
			frac1 = 10 * frac1 + (c - '0');
		}
		frac2 = 0;
		for (; mantSize > 0 && p != pEnd; mantSize -= 1)
		{
			c = *p;
			p += 1;
			if (c == '.') {
				c = *p;
				p += 1;
			}
			frac2 = 10 * frac2 + (c - '0');
		}
		fraction = (1.0e9 * frac1) + frac2;
	}

	/*
	 * Skim off the exponent.
	 */

	p = pExp;
	if ((p != pEnd) && (*p == 'E') || (*p == 'e')) {
		p += 1;

		if (p != pEnd) {
			if (*p == '-') {
				expSign = BON_TRUE;
				p += 1;
			}
			else {
				if (*p == '+') {
					p += 1;
				}
				expSign = BON_FALSE;
			}
		}

		while (p != pEnd && isdigit((unsigned char)*p)) {
			exp = exp * 10 + (*p - '0');
			p += 1;
		}
	}

	if (expSign) {
		exp = fracExp - exp;
	}
	else {
		exp = fracExp + exp;
	}

	/*
	 * Generate a floating-point number that represents the exponent.
	 * Do this by processing the exponent one bit at a time to combine
	 * many powers of 2 of 10. Then combine the exponent with the
	 * fraction.
	 */

	if (exp < 0) {
		expSign = BON_TRUE;
		exp = -exp;
	}
	else {
		expSign = BON_FALSE;
	}
	if (exp > maxExponent) {
		exp = maxExponent;
		goto fail;
	}
	dblExp = 1.0;
	for (d = powersOf10; exp != 0; exp >>= 1, d += 1) {
		if (exp & 01) {
			dblExp *= *d;
		}
	}
	if (expSign) {
		fraction /= dblExp;
	}
	else {
		fraction *= dblExp;
	}

	if (endPtr != NULL) {
		*endPtr = p;
	}

	if (sign) {
		return -fraction;
	}
	return fraction;

fail:
	if (endPtr != NULL) {
		*endPtr = string;
	}

	return 0.0;
}

static void
ParseNumberValue(BonParsedJson* pj, BonVariant* value) {
	const uint8_t* endptr = 0;

	value->value.numberValue = StringToDouble((const char*)pj->cursor, pj->jsonStringEnd - pj->cursor, (const char**)&endptr);

	if (endptr == pj->cursor) {
		GiveUp(pj->env, BON_STATUS_INVALID_NUMBER);
	}

	pj->cursor = endptr;

	value->type = BON_VT_NUMBER;
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

static int
NameCompare(const BonStringEntry* a, const BonStringEntry* b) {
	return (int)((int64_t)(uint64_t)(a->hash) - (int64_t)(uint64_t)(b->hash));
}

static void
LinkAliasesInSortedList(BonStringEntry* head) {
	BonStringEntry* p = head;
	BonStringEntry* ptop = p;
	while(p) {
		/* TODO: Should check for conflicts here */
		if (p->hash == ptop->hash) {
			p->alias = ptop;
		} else {
			ptop = p;
		}
		p = p->next;
	}
}

static size_t
ComputeStorageForUniqueStrings(BonStringEntry* head) {
	size_t size = 0;
	BonStringEntry* p = head;
	while (p) {
		if (p->alias == p) {
			size += BonRoundUp(p->byteCount + 1, 8);
		}
		p = p->next;
	}
	return size;
}

static size_t
ComputeStorageSizeForRecord(BonParsedJson* pj) {
	size_t size = 0;
	const size_t objectHeaderSize = 8;
	const size_t arrayHeaderSize = 8;
	
	/* Header */
	size += BonRoundUp(sizeof(BonRecord), 8);

	/* Objects */
	size += objectHeaderSize * pj->objectCount + sizeof(BonVariant) * pj->objectMemberCount + sizeof(BonName) * pj->objectMemberNameCount;

	/* Arrays */
	size += arrayHeaderSize * pj->arrayCount + sizeof(BonVariant)* pj->arrayMemberCount;

	/* String values */
	LinkAliasesInSortedList(pj->valueStringList);
	size += ComputeStorageForUniqueStrings(pj->valueStringList);

	/* Names */
	LinkAliasesInSortedList(pj->nameStringList);
	size += ComputeStorageForUniqueStrings(pj->nameStringList);

	return size;
}

/* Sort the parsedJson into a canonical form */
static void
CanonicalizeParsedJson(BonParsedJson* pj) {
	BonSortList(&pj->nameStringList, BonStringEntry, NameCompare);
	BonSortList(&pj->valueStringList, BonStringEntry, NameCompare);
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

		CanonicalizeParsedJson(pj);
		pj->bonRecordSize = ComputeStorageSizeForRecord(pj);
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
		BonObjectEntry* p = v->value.objectValue;
		while (p) {
			BonObjectEntry* next = p->next;
			FreeString(p->name);
			FreeVariant(&p->value);
			free(p);
			p = next;
		}
		break;
	}
	case BON_VT_ARRAY: {
		BonArrayEntry* p = v->value.arrayValue;
		while (p) {
			BonArrayEntry* next = p->next;
			FreeVariant(&p->value);
			free(p);
			p = next;
		}
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
	"+{\"apa\":false,\"Skill\":null,\"foo\":\"Hello\",\"child\":{\"apa\":96.0}}\0"
	"+[]\0"
	"+[1, 2.3, -1, 2.0e-1, 0.333e+23, 0.44E8, 123123123.4E-3]\0"
	"+[false,true,null,false,\"apa\",{},[]]\0"
	"+[false,true,null,false,\"apa\",{\"foo\":false},[\"a\",false,null]]\0"
	"-[\0"
	"-\0"
	"-25\0"
	"-[ \"abc ]\0"
	"-[false,true,null,false,\"apa\",{\"foo\":false},[\"a\",false,nul]]\0"
	"\0"; /* Terminate tests */

int 
main(int argc, char** argv) {
	const char* test = s_tests;
	int testNum = 0;
	/*
	const char test2[] = "0.232e23";
	size_t tl = sizeof(test2) - 1;
	const char* endptr = 0;
	double result = StringToDouble(test2, tl-1, &endptr);
	int failure = endptr == test2;
	(void)result;
	(void)failure;
	*/

	assert(BonCreateName("apa", 3) != BonCreateName("foo", 3));

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
			if ((parsedJson->status == BON_STATUS_OK)) {
				printf("%d: Size: %lu\n", testNum, (unsigned long)parsedJson->bonRecordSize);
			}
		}

		testNum++;
		
		FreeParsedJson(parsedJson);
		test += len + 1;
	}

	_CrtDumpMemoryLeaks();
	return 0;
}

#pragma warning(pop)
