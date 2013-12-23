#include "Bon.h"
#include "BonConvert.h"
#include "Beon.h"

#include <stdlib.h>
#ifdef _WIN32
#include <crtdbg.h>
#endif
#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

/*---------------------------------------------------------------------------*/
/* :Testing */

/* Tests starting with + are expected to succeed and those that start with - are expected to fail */
static const char s_tests[] =
	"+[false,true,null,false,\"apa\",{},[]]\0"
	"+[1, 2.3, -1, 2.0e-1, 0.333e+23, 0.44E8, 123123123.4E-3]\0"
	"+{\"apa\":false,\"Skill\":null,\"foo\":\"Hello\",\"child\":{\"apa\":96.0}}\0"
	"+[]\0"
	"+[false,true,null,false,\"apa\",{\"foo\":false},[\"a\",false,null]]\0"
	"-[\0"
	"-\0"
	"-25\0"
	"-[ \"abc ]\0"
	"-[false,true,null,false,\"apa\",{\"foo\":false},[\"a\",false,nul]]\0"
	"\0";									/* Terminate tests */

static void
ParseTests(void) {
	const char* test = s_tests;
	int testNum = 0;

	while (*test) {
		int expectedResult = (*test++ == '+') ? BON_TRUE : BON_FALSE;
		size_t len = strlen(test);

		const BonRecord* br = BonCreateRecordFromJson(test, len);
		if (!br) {
			if (expectedResult == BON_TRUE) {
				printf("FAIL (-): %s\n", test);
			}
		}
		else {
			if (expectedResult == BON_FALSE) {
				printf("FAIL (-): %s\n", test);
			}
			BonWriteAsJsonToStream(br, stdout);
			printf("\n\n");
			free((void*)br);
		}

		testNum++;
		test += len + 1;
	}
}

uint8_t* 
LoadAll(size_t* filesizeOut, const char* fn) {
	FILE* f;
	uint8_t* docBufData;
	if (filesizeOut)
		*filesizeOut = 0;

	f = fopen(fn, "rb");
	if (!f)
		return 0;
	fseek(f, 0, SEEK_END);
	long filesize = ftell(f);
	fseek(f, 0, SEEK_SET);

	docBufData = (uint8_t*)malloc((size_t)filesize);
	if (!docBufData)
		return 0;

	fread(docBufData, (size_t)filesize, 1, f);
	fclose(f);

	if (filesizeOut)
		*filesizeOut = (size_t)filesize;

	return docBufData;
}

typedef struct LinearAllocator {
	uint8_t*		mem;
	uint8_t*		memEnd;
	uint8_t*		cursor;
} LinearAllocator;

LinearAllocator g_linearAllocator;

static size_t
DoRoundUp(size_t value, size_t multiple) {
	if (multiple == 0) {
		return value;
	}
	else {
		const size_t remainder = value % multiple;
		if (remainder == 0)
			return value;
		return value + multiple - remainder;
	}
}


static void*
LinearAlloc(void* userdata, size_t size) {
	LinearAllocator* allocator = (LinearAllocator*)userdata;
	size = DoRoundUp(size, 8);
	if (allocator->cursor + size <= allocator->memEnd) {
		void* r = allocator->cursor;
		allocator->cursor += size;
		return r;
	} else {
		return 0;
	}
}

BonRecord*
TestCreateRecordFromJsonUsingLinearAllocator(const char* jsonString, size_t jsonStringByteCount) {
	struct BonParsedJson*	parsedJson;
	BonRecord*		bonRecord = 0;

	parsedJson = BonParseJson(LinearAlloc, &g_linearAllocator, jsonString, jsonStringByteCount);
	if (BonGetParsedJsonStatus(parsedJson) != BON_STATUS_OK) {
		g_linearAllocator.cursor = g_linearAllocator.mem;
		return 0;
	}

	/*bonRecord = BonCreateRecordFromParsedJson(parsedJson, malloc(BonGetBonRecordSize(parsedJson)));
	printf ("Used mem %u\n", (unsigned)(g_linearAllocator.cursor - g_linearAllocator.mem));*/
	g_linearAllocator.cursor = g_linearAllocator.mem;

	return bonRecord;
}

#define LA
void
BigTest(void) {
	int i;
	clock_t start, end;
	size_t jsonSize = 0;
	uint8_t* json = LoadAll(&jsonSize, 
		"c:\\Users\\Jonas\\Proj\\E2\\Data\\Kristian_skeleton.json"
/*		"c:\\Users\\Jonas\\Proj\\E2\\Test\\bigtest.json"*/
		);

	const size_t		memSize = 1024 * 1024 * 256;
	g_linearAllocator.mem = malloc(memSize);
	g_linearAllocator.memEnd = g_linearAllocator.mem + memSize;
	g_linearAllocator.cursor = g_linearAllocator.mem;

	assert(json);

	start = clock();
	for (i = 0; i < 1000; ++i) {
#ifdef LA
		BonRecord* br = TestCreateRecordFromJsonUsingLinearAllocator((const char*)json, jsonSize);
#else
		BonRecord* br = BonCreateRecordFromJson((const char*)json, jsonSize);
#endif
		if (br)
			free(br);
	}
	free(json);
	free(g_linearAllocator.mem);

	end = clock();
	printf("Elapsed: %8.4f\n", (float)(end - start)/CLOCKS_PER_SEC);
}

int 
main(int argc, char** argv) {
	ParseTests();
	/*BigTest();*/
#ifdef _WIN32
	_CrtDumpMemoryLeaks();
#endif
	return 0;
}

