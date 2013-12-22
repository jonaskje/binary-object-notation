#include "Bon.h"
#include "BonConvert.h"
#include "Beon.h"

#include <stdlib.h>
#include <crtdbg.h>
#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>

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

int 
main(int argc, char** argv) {
	ParseTests();
	_CrtDumpMemoryLeaks();
	return 0;
}

