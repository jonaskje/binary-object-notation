#include "Bon.h"

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

/*---------------------------------------------------------------------------*/
/* :Reading */

const char*
BonGetNameString(BonRecord* br, BonName name) {
	assert(0);
	return 0;
}


#pragma warning(pop)
