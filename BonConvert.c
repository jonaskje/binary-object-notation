#include "BonConvert.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <ctype.h>

/*---------------------------------------------------------------------------*/
/* List helpers */

#define BonPrependToList(head, obj) \
	do { \
		(obj)->next = *(head); \
		*(head) = obj; \
	} while (0)

/* Standard isdigit was too expensive (about 20% of total time when parsing a number-dense file) */
#define IsDigit(c) (((c) >= (uint8_t)'0') & ((c) <= (uint8_t)'9'))

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
#define BonSortList(head, ElementType, CompareFun)				\
	do {	                                                                \
		ElementType *p, *q, *e, *tail, *oldhead, *list;			\
		int insize, nmerges, psize, qsize, i;				\
		list = *(head);							\
		if (!list) break;						\
		insize = 1;							\
		for (;;) {							\
			p = oldhead = list;					\
			list = tail = 0;					\
			nmerges = 0;						\
			while (p) {						\
				nmerges++;					\
				q = p;						\
				psize = 0;					\
				for (i = 0; i < insize; i++) {			\
					psize++;				\
					q = q->next;				\
					if (!q) break;				\
				}						\
				qsize = insize;					\
				while (psize > 0 || (qsize > 0 && q)) {		\
					if (psize == 0) {			\
						e = q; q = q->next; qsize--;	\
					}					\
					else if (qsize == 0 || !q) {		\
						e = p; p = p->next; psize--;	\
					}					\
					else if (CompareFun(p, q) <= 0) {	\
						e = p; p = p->next; psize--;	\
					}					\
					else {					\
						e = q; q = q->next; qsize--;	\
					}					\
					if (tail) {				\
						tail->next = e;			\
					}					\
					else {					\
						list = e;			\
					}					\
					tail = e;				\
				}						\
				p = q;						\
			}							\
			tail->next = NULL;					\
			if (nmerges <= 1) {					\
				*(head) = list;					\
				break;						\
			}							\
			insize *= 2;                                            \
		}								\
	} while(0)

/*---------------------------------------------------------------------------*/
/* Internal */

struct BonObjectEntry;
struct BonArrayEntry;

typedef struct BonStringEntry {
	struct BonStringEntry*	next;
	struct BonStringEntry*	alias;
	size_t			offset;
	size_t			byteCount;
	BonName			hash;
	uint8_t			utf8[1];
} BonStringEntry;

typedef struct BonContainer {
	struct BonContainer*	next;
	int			type;
} BonContainer;

typedef struct BonArrayHead {
	BonContainer		container;
	size_t			offset;
	size_t			size;
	struct BonArrayEntry*	valueList;
	struct BonArrayEntry**	lastValue;
} BonArrayHead;

typedef struct BonObjectHead {
	BonContainer		container;
	size_t			offset;
	size_t			size;
	size_t			memberCount;
	struct BonObjectEntry*	memberList;
} BonObjectHead;

typedef struct BonVariant {
	union {
		BonBool			boolValue;
		double			numberValue;
		struct BonObjectHead*	objectValue;
		struct BonArrayHead*	arrayValue;
		BonStringEntry*		stringValue;
		BonValue		value;
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
	BonTempMemoryAlloc	alloc;
	const uint8_t*		jsonString;
	const uint8_t*		jsonStringEnd;

	/* Parse context */
	const uint8_t*		cursor;
	jmp_buf*		env;

	BonStringEntry*		nameStringList;
	BonStringEntry*		valueStringList;
	BonContainer*		containerList;
	BonContainer**		lastContainer;

	size_t			totalObjectSize;
	size_t			totalArraySize;
	size_t			totalValueStringSize;
	size_t			totalNameLookupSize;
	size_t			totalNameStringSize;

	size_t			totalNameStringCount;

	size_t			objectOffset;
	size_t			arrayOffset;
	size_t			valueStringOffset;
	size_t			nameLookupOffset;
	size_t			nameStringOffset;

	void*			recordBaseMemory;

	/* Results */
	int			status;
	size_t			bonRecordSize;
	BonVariant		rootValue;
} BonParsedJson;

typedef struct BonContainerInternal {
	int32_t		capacity;	/* Negative for objects and positive for arrays */
	int32_t		count;
	BonValue	items[1];
} BonContainerInternal;

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
DoTempCalloc(BonTempMemoryAlloc alloc, jmp_buf* exceptionEnv, size_t size) {
	void* result = alloc(size);
	if (!result) {
		GiveUp(exceptionEnv, BON_STATUS_OUT_OF_MEMORY);
	}
	return memset(result, 0, size);
}

#define BonFourCC(a,b,c,d)		((uint32_t) (((d)<<24) | ((c)<<16) | ((b)<<8) | (a)))

#define BonTempCalloc(tempAllocator, exceptionEnv, type) ((type*)DoTempCalloc(tempAllocator, exceptionEnv, sizeof(type)))

static BonObjectEntry*
CreateObjectEntry(BonParsedJson* pj) {
	return BonTempCalloc(pj->alloc, pj->env, BonObjectEntry);
}

static BonArrayEntry*
CreateArrayEntry(BonParsedJson* pj) {
	return BonTempCalloc(pj->alloc, pj->env, BonArrayEntry);
}

static BonObjectHead*
InitObjectVariant(BonParsedJson* pj, BonVariant* v) {
	BonObjectHead* head = BonTempCalloc(pj->alloc, pj->env, BonObjectHead);
	head->container.type = BON_VT_OBJECT;
	v->type = BON_VT_OBJECT;
	v->value.objectValue = head;
	return head;
}

static BonArrayHead*
InitArrayVariant(BonParsedJson* pj, BonVariant* v) {
	BonArrayHead* head = BonTempCalloc(pj->alloc, pj->env, BonArrayHead);
	head->lastValue = &head->valueList;
	head->container.type = BON_VT_ARRAY;
	v->type = BON_VT_ARRAY;
	v->value.arrayValue = head;
	return head;
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
		+(stringEnd - string)						/* Min length of string in bytes */
		+ 1;								/* Space for a terminating null */

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
			case 0x22u:						/* \" */
			case 0x5Cu:						/* \\ */
			case 0x2Fu:						/* \/ */
				*dstString++ = c2;
				break;
			case 0x62u:						/* \b */
				*dstString++ = +0x08u;
				break;
			case 0x66u:						/* \f */
				*dstString++ = +0x0Cu;
				break;
			case 0x6Eu:						/* \n */
				*dstString++ = +0x0Au;
				break;
			case 0x72u:						/* \r */
				*dstString++ = +0x0Du;
				break;
			case 0x74u:						/* \t */
				*dstString++ = +0x09u;
				break;
			case 0x75u:						/* \u */
				assert(0);					/* TODO: Unicode escape */
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
static void			ParseObject(BonParsedJson* pj, BonObjectHead* objectHead);
static void			ParseArray(BonParsedJson* pj, BonArrayHead* arrayHead);

static BonVariant		s_nullVariant		= { 0, BON_VT_NULL };
static BonVariant		s_boolFalseVariant	= { BON_FALSE, BON_VT_BOOL };
static BonVariant		s_boolTrueVariant	= { BON_TRUE, BON_VT_BOOL };

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
ParseObject(BonParsedJson* pj, BonObjectHead* objectHead) {
	int memberCount = 0;
	FailIfEof(pj);
	FailUnlessCharIs(pj, '{');
	SkipWhitespace(pj);
	if (PeekChar(pj, '}')) {
		goto done;
	}
	for(;;) {
		BonObjectEntry* member = AppendObjectMember(pj, &objectHead->memberList);
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

	/* Canonicalize */
	BonSortList(&objectHead->memberList, BonObjectEntry, ObjectNameCompare);
done:
	FailUnlessCharIs(pj, '}');
	objectHead->size = 8;							/* Size of the object header */
	objectHead->size += memberCount * sizeof(BonValue);
	objectHead->size += BonRoundUp(memberCount, 2) * sizeof(BonName);	/* Account for round up when odd number of members */
	objectHead->memberCount = memberCount;
}

static BonArrayEntry*
AppendArrayMember(BonParsedJson* pj, BonArrayHead* arrayHead) {
	BonArrayEntry* member = CreateArrayEntry(pj);
	member->value = s_nullVariant;
	*arrayHead->lastValue = member;
	arrayHead->lastValue = &member->next;
	return member;
}

static void
ParseArray(BonParsedJson* pj, BonArrayHead* arrayHead) {
	int memberCount = 0;
	FailIfEof(pj);
	FailUnlessCharIs(pj, '[');
	SkipWhitespace(pj);
	if (PeekChar(pj, ']')) {
		goto done;
	}
	for (;;) {
		BonArrayEntry* member = AppendArrayMember(pj, arrayHead);

		memberCount++;

		ParseValue(pj, &member->value);
		SkipWhitespace(pj);
		if (!PeekChar(pj, ','))
			break;
		FailUnlessCharIs(pj, ',');
		SkipWhitespace(pj);
	}
done:
	FailUnlessCharIs(pj, ']');
	arrayHead->size = 8;							/* Size of the array header */
	arrayHead->size += memberCount * sizeof(BonValue);
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
static int maxExponent = 511;							/* Largest possible base 10 exponent.  Any
										 * exponent larger than this will already
										 * produce underflow or overflow, so there's
										 * no need to worry about additional digits.
										 */

static double powersOf10[] = {							/* Table giving binary powers of 10.  Entry */
	10.,									/* is 10^2^i.  Used to convert decimal */
	100.,									/* exponents into floating-point numbers. */
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
	int exp = 0;								/* Exponent read from "EX" field. */
	int fracExp = 0;							/* Exponent that derives from the fractional
										 * part.  Under normal circumstances, it is
										 * the negative of the number of digits in F.
										 * However, if I is very long, the last digits
										 * of I get dropped (otherwise a long I with a
										 * large negative exponent could cause an
										 * unnecessary overflow on I alone).  In this
										 * case, fracExp is incremented one for each
										 * dropped digit. */
	int mantSize;								/* Number of digits in mantissa. */
	int decPt;								/* Number of mantissa digits BEFORE decimal
										 * point. */
	const char *pExp;							/* Temporarily holds location of exponent
										 * in string. */
	const char *pEnd;							/* Max position of p */

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
		if (!IsDigit(c)) {
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
		mantSize -= 1;							/* One of the digits was the point. */
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
	if ((p != pEnd) && ((*p == 'E') || (*p == 'e'))) {
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

		while (p != pEnd && IsDigit((unsigned char)*p)) {
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

static size_t
ComputeOffsetAndLinkAliasesInSortedList(size_t* stringCount, BonStringEntry* head) {
	size_t			totalSize	= 0;
	size_t			count		= 0;
	BonStringEntry*		p		= head;
	BonStringEntry*		ptop		= p;

	while(p) {
		/* TODO: Should check for conflicts here */
		if (p->hash == ptop->hash) {
			p->alias = ptop;
		} else {
			ptop = p;
		}
		if (p == p->alias) {
			p->offset = totalSize;
			totalSize += BonRoundUp(p->byteCount + 1, 8);
			++count;
		}
		p = p->next;
	}
	if (stringCount) {
		*stringCount = count; 
	}	
	return totalSize;
}

static size_t
ComputeStorageSizeForRecord(BonParsedJson* pj) {
	size_t size = 0;
	size += BonRoundUp(sizeof(BonRecord), 8);

	pj->objectOffset = size;
	size += pj->totalObjectSize;

	pj->arrayOffset = size;
	size += pj->totalArraySize;

	pj->valueStringOffset = size;
	size += pj->totalValueStringSize;

	pj->nameLookupOffset = size;
	size += pj->totalNameLookupSize;

	pj->nameStringOffset = size;
	size += pj->totalNameStringSize;

	return size;
}

static void
ComputeVariantOffsets(BonParsedJson* pj) {
	/* Exploit fact that both arrayValue and objectValue has a BonContainer as the first member */
	BonContainer*		rootContainer	= &(pj->rootValue.value.objectValue->container);
	BonContainer*		p		= rootContainer;
	size_t			totalObjectSize = 0;
	size_t			totalArraySize	= 0;

	*pj->lastContainer = rootContainer;
	assert(rootContainer->next == 0);
	pj->lastContainer = &(rootContainer->next);

	for (; p; p = p->next) {
		if (p->type == BON_VT_OBJECT) {
			BonObjectHead* head = (BonObjectHead*)p;
			BonObjectEntry* entry;
			head->offset = totalObjectSize;
			totalObjectSize += head->size;
			for (entry = head->memberList; entry; entry = entry->next) {
				if (entry->value.type == BON_VT_OBJECT || entry->value.type == BON_VT_ARRAY) {
					BonContainer* entryContainer = &entry->value.value.objectValue->container;
					*pj->lastContainer = entryContainer;
					pj->lastContainer = &(entryContainer->next);
				}
			}
		} else {
			BonArrayHead* head = (BonArrayHead*)p;
			BonArrayEntry* entry;
			assert(p->type == BON_VT_ARRAY);
			head->offset = totalArraySize;
			totalArraySize += head->size;
			for (entry = head->valueList; entry; entry = entry->next) {
				if (entry->value.type == BON_VT_OBJECT || entry->value.type == BON_VT_ARRAY) {
					BonContainer* entryContainer = &entry->value.value.objectValue->container;
					*pj->lastContainer = entryContainer;
					pj->lastContainer = &(entryContainer->next);
				}
			}
		}
	}

	pj->totalObjectSize	= totalObjectSize;
	pj->totalArraySize	= totalArraySize;
}

BonParsedJson*
BonParseJson(BonTempMemoryAlloc tempAllocator, const char* jsonString, size_t jsonStringByteCount) {
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
		pj->lastContainer	= &pj->containerList;

		if (!jsonString || jsonStringByteCount < 2) {
			GiveUp(pj->env, BON_STATUS_INVALID_JSON_TEXT);
		}

		/* http://www.ietf.org/rfc/rfc4627.txt, section 3. Encoding
		 * http://en.wikipedia.org/wiki/Byte_order_mark, 
		 * Verify that string is UTF8 by checking for nulls in the first two bytes and also for non UTF8 BOMs*/
		if (pj->jsonString[0] == 0 || pj->jsonString[1] == 0 || pj->jsonString[0] == 0xFEu || pj->jsonString[0] == 0xFFu) {
			GiveUp(pj->env, BON_STATUS_JSON_NOT_UTF8);
		}

		/* Skip three byte UTF-8 BOM if there is one */
		if (pj->jsonString[0] == 0xEFu) {
			if (jsonStringByteCount < 3 || pj->jsonString[1] != 0xBBu || pj->jsonString[2] != 0xBFu) {
				GiveUp(pj->env, BON_STATUS_INVALID_JSON_TEXT);
			}

			pj->cursor += 3;					/* Skip parsing BOM */
		}

		SkipWhitespace(pj);
		ParseObjectOrArray(pj);
		SkipWhitespace(pj);

		if (pj->cursor != pj->jsonStringEnd) {
			GiveUp(pj->env, BON_STATUS_JSON_PARSE_ERROR);
		}

		/* Sort the strings by hash into a canonical form */
		BonSortList(&pj->nameStringList, BonStringEntry, NameCompare);
		pj->totalNameStringSize = ComputeOffsetAndLinkAliasesInSortedList(&pj->totalNameStringCount, pj->nameStringList);
		pj->totalNameLookupSize = 8;
		pj->totalNameLookupSize += pj->totalNameStringCount * (sizeof(BonName) + sizeof(uint32_t)); /* Name, offset pair */

		BonSortList(&pj->valueStringList, BonStringEntry, NameCompare);
		pj->totalValueStringSize = ComputeOffsetAndLinkAliasesInSortedList(0, pj->valueStringList);

		ComputeVariantOffsets(pj);

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

static ptrdiff_t 
RelativeOffset(void* from, void* basePtr, size_t offsetFromBase) {
	return ((uint8_t*)basePtr + offsetFromBase) - (uint8_t*)from;
}

static BonValue
MakeObjectValue(ptrdiff_t relativeOffset) {
	assert((relativeOffset & 0x7ll) == 0);
	return ((BonValue)relativeOffset << 32) | (BonValue)BON_VT_OBJECT;
}

static BonValue
MakeArrayValue(ptrdiff_t relativeOffset) {
	assert((relativeOffset & 0x7ll) == 0);
	return ((BonValue)relativeOffset << 32) | (BonValue)BON_VT_ARRAY;
}

static BonValue
MakeStringValue(ptrdiff_t relativeOffset) {
	assert((relativeOffset & 0x7ll) == 0);
	return ((BonValue)relativeOffset << 32) | (BonValue)BON_VT_STRING;
}

static BonValue
MakeBoolValue(int v) {
	return ((BonValue)v << 32) | (BonValue)BON_VT_BOOL;
}

static BonValue
MakeNullValue(void) {
	return (BonValue)0x7ull;
}

static BonValue
MakeNumberValue(const BonVariant* v) {
	return (v->value.value & ~0x7ull);
}

static BonValue
MakeValueFromVariant(BonParsedJson* pj, BonValue* value, BonVariant* v) {
	switch (v->type) {
	case BON_VT_NUMBER:	return MakeNumberValue(v);
	case BON_VT_BOOL:	return MakeBoolValue(v->value.boolValue);
	case BON_VT_STRING:	return MakeStringValue(RelativeOffset(value, pj->recordBaseMemory, pj->valueStringOffset + v->value.stringValue->alias->offset));
	case BON_VT_ARRAY:	return MakeArrayValue(RelativeOffset(value, pj->recordBaseMemory, pj->arrayOffset + v->value.arrayValue->offset));
	case BON_VT_OBJECT:	return MakeObjectValue(RelativeOffset(value, pj->recordBaseMemory, pj->objectOffset + v->value.objectValue->offset));
	case BON_VT_NULL:	return MakeNullValue();
	default: assert(0);	return 0;
	}
}


BonRecord*		
BonCreateRecordFromParsedJson(BonParsedJson* pj, void* recordMemory) {
	/* Exploit fact that both arrayValue and objectValue has a BonContainer as the first member */
	BonContainer*		p		= pj->containerList;
	BonRecord*		header		= (BonRecord*)recordMemory;
	uint8_t*		baseMemory	= (uint8_t*)recordMemory;
	BonStringEntry*		stringEntry	= 0;
	uint32_t*		nameLookupCursor= (uint32_t*)(baseMemory + pj->nameLookupOffset);

	assert(pj->status == BON_STATUS_OK);

	pj->recordBaseMemory = recordMemory;

	/* Header */
	header->magic			= BonFourCC('B', 'O', 'N', ' ');
	header->flags			= 0;
	header->reserved0		= 0;
	header->reserved1		= 0;
	header->version			= 0;
	header->nameLookupTableOffset	= (int32_t)RelativeOffset(&header->nameLookupTableOffset, baseMemory, pj->nameLookupOffset);
	header->rootValue		= MakeValueFromVariant(pj, &header->rootValue, &pj->rootValue);

	/* Containers and arrays */
	for (; p; p = p->next) {
		if (p->type == BON_VT_OBJECT) {
			BonObjectHead*		head	= (BonObjectHead*)p;
			BonContainerInternal*	dst	= (BonContainerInternal*)(baseMemory + head->offset + pj->objectOffset);
			BonName*		name	= (BonName*)(&(dst->items[head->memberCount]));
			BonValue*		item	= &(dst->items[0]);
			BonObjectEntry*		entry;

			dst->count	= (int32_t)head->memberCount;
			dst->capacity	= -dst->count;
			for (entry = head->memberList; entry; entry = entry->next) {
				*name++ = entry->name->hash;
				*item = MakeValueFromVariant(pj, item, &entry->value);
				++item;
			}
			if (dst->count % 2) {
				*name++ = 0;					/* Clear the odd name slot (everything is 8 byte aligned) */
			}
		}
		else {
			BonArrayHead*		head = (BonArrayHead*)p;
			BonContainerInternal*	dst = (BonContainerInternal*)(baseMemory + head->offset + pj->arrayOffset);
			BonValue*		item = &(dst->items[0]);
			BonArrayEntry*		entry;
			assert(p->type == BON_VT_ARRAY);
			dst->count	= (int32_t)((head->size - 8) / sizeof(BonValue));
			dst->capacity	= dst->count;
			for (entry = head->valueList; entry; entry = entry->next) {
				*item = MakeValueFromVariant(pj, item, &entry->value);
				++item;
			}
		}
	}

	/* Value strings */
	for (stringEntry = pj->valueStringList; stringEntry; stringEntry = stringEntry->next) {
		uint8_t*	dst;
		size_t		zeroCount;
		/* Ignore all with an alias */
		if (stringEntry->alias != stringEntry) {
			continue;
		}
		dst = baseMemory + pj->valueStringOffset + stringEntry->offset;
		memcpy(dst, stringEntry->utf8, stringEntry->byteCount);
		dst += stringEntry->byteCount;
		zeroCount = BonRoundUp(stringEntry->byteCount, 8) - stringEntry->byteCount;
		memset(dst, 0, zeroCount);
	}

	/* Name lookup and name strings*/
	*nameLookupCursor++ = (uint32_t)pj->totalNameStringCount; /* capacity */
	*nameLookupCursor++ = (uint32_t)pj->totalNameStringCount; /* count: Same as capacity */
	for (stringEntry = pj->nameStringList; stringEntry; stringEntry = stringEntry->next) {
		uint8_t*	dst;
		size_t		zeroCount;
		/* Ignore all with an alias */
		if (stringEntry->alias != stringEntry) {
			continue;
		}
		
		/* Write a (name hash, offset) pair for each name */
		*nameLookupCursor++ = stringEntry->hash;
		*nameLookupCursor = (uint32_t)RelativeOffset(nameLookupCursor, baseMemory, pj->nameStringOffset + stringEntry->offset);
		++nameLookupCursor;

		dst = baseMemory + pj->nameStringOffset + stringEntry->offset;
		memcpy(dst, stringEntry->utf8, stringEntry->byteCount);
		dst += stringEntry->byteCount;
		zeroCount = BonRoundUp(stringEntry->byteCount, 8) - stringEntry->byteCount;
		memset(dst, 0, zeroCount);
	}

	return header;
}

static void
FreeString(BonStringEntry* v, BonTempMemoryFree freeFun) {
	if (!v)
		return;
	freeFun(v);
}

static void 
FreeVariant(BonVariant* v, BonTempMemoryFree freeFun) {
	switch (v->type) {
	case BON_VT_OBJECT: {
		BonObjectHead* head = v->value.objectValue;
		BonObjectEntry* p;
		if (!head)
			break;
		p = head->memberList;
		freeFun(head);
		while (p) {
			BonObjectEntry* next = p->next;
			FreeString(p->name, freeFun);
			FreeVariant(&p->value, freeFun);
			freeFun(p);
			p = next;
		}
		break;
	}
	case BON_VT_ARRAY: {
		BonArrayHead* head = v->value.arrayValue;
		BonArrayEntry* p;
		if (!head)
			break;
		p = head->valueList;
		freeFun(head);
		while (p) {
			BonArrayEntry* next = p->next;
			FreeVariant(&p->value, freeFun);
			freeFun(p);
			p = next;
		}
		break;
	}
	case BON_VT_STRING: {
		FreeString(v->value.stringValue, freeFun);
		break;
	}

	}
}

void 
BonFreeParsedJsonMemory(BonParsedJson* parsedJson, BonTempMemoryFree freeFun) {
	if (parsedJson) {
		FreeVariant(&parsedJson->rootValue, freeFun);
		freeFun(parsedJson);
	}
}

BonRecord*		
BonCreateRecordFromJson(const char* jsonString, size_t jsonStringByteCount) {
	BonParsedJson*		parsedJson	= BonParseJson(malloc, jsonString, jsonStringByteCount);
	BonRecord*		bonRecord;

	if (parsedJson->status != BON_STATUS_OK) {
		BonFreeParsedJsonMemory(parsedJson, free);
		return 0;
	}

	bonRecord = BonCreateRecordFromParsedJson(parsedJson, malloc(BonGetBonRecordSize(parsedJson)));
	BonFreeParsedJsonMemory(parsedJson, free);

	return bonRecord;
}

/*---------------------------------------------------------------------------*/
/* Output */

typedef struct JSONPrinter {
	const BonRecord*	doc;
	int			indent;
	FILE*			stream;
} JSONPrinter;


static const char* 
IndentStr(int indent) {
	static const char empty[] = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";
	const char* indentStr = empty + sizeof(empty)-1;
	indentStr -= indent;
	if (indentStr < empty) {
		indentStr = empty;
	}
	return indentStr;
}

static void 
printAsJSON(JSONPrinter* p, const BonValue* v, BonBool lastInList, const char* IndentString)
{
	if (BonIsNullValue(v)) {
		fprintf(p->stream, "%snull", IndentString);
	} else {
		switch (BonGetValueType(v)) {
		case BON_VT_BOOL:
			fprintf(p->stream, "%s%s", IndentString, (BonAsBool(v) ? "true" : "false"));
			break;
		case BON_VT_NUMBER:
			fprintf(p->stream, "%s%f", IndentString, BonAsNumber(v));
			break;
		case BON_VT_STRING:
			fprintf(p->stream, "%s\"%s\"", IndentString, BonAsString(v));
			break;
		case BON_VT_ARRAY: {
			BonArray array = BonAsArray(v);
			int i;
			fprintf(p->stream, "%s[\n", IndentString);
			p->indent++;
			for (i = 0; i < array.count; ++i) {
				printAsJSON(p, &array.values[i], i == array.count - 1, IndentStr(p->indent));
			}
			p->indent--;
			fprintf(p->stream, "%s]", IndentStr(p->indent));
			break;
		}
		case BON_VT_OBJECT: {
			BonObject object = BonAsObject(v);
			int i;
			fprintf(p->stream, "%s{\n", IndentString);
			p->indent++;
			for (i = 0; i < object.count; ++i) {
				const char* nameString = BonGetNameString(p->doc, object.names[i]);
				fprintf(p->stream, "%s\"%s\" : ", IndentStr(p->indent), nameString);
				printAsJSON(p, &object.values[i], i == object.count - 1, "");
			}
			p->indent--;
			fprintf(p->stream, "%s}", IndentStr(p->indent));
			break;
		}
		default:
			fprintf(p->stream, "%s!!!invalid!!!", IndentString);
			break;
		}
	}
	if (lastInList) {
		fprintf(p->stream, "\n");
	} else {
		fprintf(p->stream, ",\n");
	}
}

void				
BonWriteAsJsonToStream(const BonRecord* record, FILE* stream) {
	JSONPrinter printer;
	printer.doc		= record;
	printer.indent		= 0;
	printer.stream		= stream;
	printAsJSON(&printer, BonGetRootValue(record), BON_TRUE, "");
}
