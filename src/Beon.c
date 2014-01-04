#include "Beon.h"
#include "Bon.h"



#if 0
void test(void) {
        BeonRecord* r = BeonCreateRecord();
        BeonValue* v = BeonGetRootValue(r);
        bool b = BeonAsBool(v);
        BeonSetBool(v, BON_TRUE);

        BeonAsArray

        BeonArray* array = BeonSetArray(BeonGetRootValue(r));
        BeonArray* array = BeonSetArrayAndCapacity(BeonGetRootValue(r), 64);
        BeonArray* array = BeonAsArray(BeonGetRootValue(r));
        
        
        int length = BeonGetArrayLength(array);
        int cap = BeonGetArrayCapacity(array);

        BeonArraySetNumber(array, index, 2.0);
        BeonArraySetNumberArray(array, index, doubles, count);
        BeonArray* array = BeonArraySetArray(array, index);
        BeonArray* array = BeonArrayAsArray(array, index);
        BeonArraySetBool(array, index, BON_TRUE);
        //etc
        
        BeonObjectSetNumber(array, hash, 2.0);
        BeonObjectSetNumberArray(array, hash, doubles, count);
        
        BonRecord* r2 = BeonFinalizeRecord(r);
}

struct BeonValue {
        struct BeonValue* parent;
        BonValue value;
} BeonValue;

#endif
