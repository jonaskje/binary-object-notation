all: test tools

O=BonConvert.o Bon.o
D=BonConvert.h Bon.h

CC=clang
CFLAGS=-g

%.o : %.c $D
	$(CC) -c $(CFLAGS) $< -o $@

test: $O BonTest.c
	$(CC) $(CFLAGS) $^ -o BonTest

tools: $O BonTools.c
	$(CC) $(CFLAGS) -DBONTOOL_JSON2BON $^ -o Json2Bon
	$(CC) $(CFLAGS) -DBONTOOL_BON2JSON $^ -o Bon2Json
	$(CC) $(CFLAGS) -DBONTOOL_DUMPBON  $^ -o DumpBon

clean:
	-rm BonTest 
	-rm Json2Bon
	-rm Bon2Json
	-rm DumpBon
	-rm BonConvert.o
	-rm Bon.o
	-rm BonTest.o

