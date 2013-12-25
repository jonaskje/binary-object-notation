#include "Bon.h"
#include "BonConvert.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

static uint8_t* 
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

static BonBool 
WriteRecordToDiskAsJson(const BonRecord* br, const char* fn) {
	FILE* f = fopen(fn, "wb");
	if (!f)
		return BON_FALSE;
	BonWriteAsJsonToStream(br, f);
	fclose(f);
	return BON_TRUE;
}

static BonBool 
WriteRecordToDisk(const BonRecord* br, const char* fn) {
	FILE* f = fopen(fn, "wb");
	if (!f)
		return BON_FALSE;
	fwrite(br, br->recordSize, 1, f);
	fclose(f);
	return BON_TRUE;
}

#ifdef BONTOOL_BONDUMP
void
Usage() {
	printf ("Dump a BON record in a raw format.\nUsage: BonDump <input bon-file>\n");
	exit(-1);
}

int 
main(int argc, char** argv) {
	uint8_t* bonData = 0;
	size_t bonDataSize;
	BonRecord* record = 0;

	if (argc != 2) 
		Usage();
	bonData = LoadAll(&bonDataSize, argv[1]);
	if (!bonData)
		Usage();

	record = (BonRecord*)bonData;
	
	if (!BonIsAValidRecord(record, bonDataSize)) {
		fprintf(stderr, "Invalid BON record\n");
		exit(-2);
	}

	BonDebugWrite(record, stdout);
	return 0;
}
#endif

#ifdef BONTOOL_JSON2BON
void
Usage() {
	printf ("Convert a JSON file to a BON record.\nUsage: Json2Bon <input json-file> <output bon-file>\n");
	exit(-1);
}

int 
main(int argc, char** argv) {
	uint8_t* jsonData;
	size_t jsonDataSize;
	BonRecord* record;

	if (argc != 3) 
		Usage();
	jsonData = LoadAll(&jsonDataSize, argv[1]);
	if (!jsonData)
		Usage();
	
	record = BonCreateRecordFromJson((const char*)jsonData, jsonDataSize);
	
	free(jsonData);

	if (!record) {
		fprintf(stderr, "Failed to parse JSON file\n");
		exit(-2);
	}
	if (!WriteRecordToDisk(record, argv[2]))
		Usage();

	free(record);

	return 0;
}
#endif

