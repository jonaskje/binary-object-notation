default: test

all: test tools

test:
	clang -g BonConvert.c Bon.c BonTest.c -o BonTest

tools:
	clang -g -DBONTOOL_JSON2BON BonConvert.c Bon.c BonTools.c -o Json2Bon
	clang -g -DBONTOOL_BONDUMP BonConvert.c Bon.c BonTools.c -o BonDump
