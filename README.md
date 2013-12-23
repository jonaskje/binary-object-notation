BON - Binary Object Notation
============================

Abstract
--------

BON is an binary and complete representation of JSON with the following potentially interesting
features:
- A BON record is memory mappable and mem moveable. No parsing or fixup required.
- A BON record is canonical in the sense that two semantically identical JSON documents are
  represented by identical BON records. 
- Homogenous arrays of numbers can be accessed as an array of doubles.
- Object members are sorted by their keys's 32-bit murmur3 hashes. This makes linear parsing of an
  object's members possible. Searching for a key is log2(N), too.

BON is not a serialization format like BSON.

1\. Introduction
---------------

JSON is interesting and useful as an interchange format due to its simplicity and widespread use.
Especially when considering doing anything in a browser with javascript.

JSON is also a text based format which is not at all good for machine reading. The parsing portion
is a big part when working with large JSON files.

The idea behind BON is to provide is a binary representation of a JSON file which can be used without any parsing
at all.

What this project provides is:
- A specification of the BON record format.
- A C implementation for reading a BON record (Bon.h/Bon.c)
- A C implementation for converting from JSON to a BON record and vice versa (BonConvert.h/BonConvert.c)
- A C implementation for editing BON records without falling back to JSON. (Beon.h/Beon.c)

Here are some ideas where this could be useful:
- Caching parsed JSON files as BON records for faster loads.
- Using BON records for storing and working with data and falling back to JSON only when it is
  needed for interop.
- Using BON records instead of JSON as runtime data as a faster alternative to JSON (for simpler
  games, for example).

2\. BON Format
-------------

A BON record consists of the following sections:
1. Header
2. Arrays
3. Objects
4. Value strings
5. Name to string lookup table
6. Name strings

A BON record is stored in little-endian format. Support for big-endian could easily be added by
checking the magic value in the header.

A JSON text is canonicalized into a BON record in the following way:
- Members in objects are ordered by the hash of their key string - ascending order.
- Arrays and objects are ordered breadth first. Arrays in the arrays section and objects in the
  objects section. E.g. the text { "a" : { "c": null }, "b" : { "d" : { "e" : null} }} has the following objects
  in the objects section:
  - 0: { "b": @1, "a" : @2} 
  - 1: { "c": null } 
  - 2: { "d": @3 } 
  - 3: { "e": null } 
  @ represents a reference to another object in the object array. "b" is before "a" because the hash value for "b" is lower than for "a".
- No duplicates of value strings are stored.
- No duplicates of name strings.
- There may be an identical name string and value string, though. The reason for that is that the
  last two sections are not necessary to understand or use a record, so dropping them could be a
  space saving alternative for some applications.

### 2.1 Values ###

All values are represented by a 64-bit value named BonValue. It is interpreted differently based
on the type. The type is stored in the 3 least significant bits of the 64-bit value.

~~~
#define BON_VT_NUMBER		0
#define BON_VT_BOOL		1
#define BON_VT_STRING		2
#define BON_VT_ARRAY		3
#define BON_VT_OBJECT		4
#define BON_VT_NULL		7
~~~

#### 2.1.1 Number Value ###

A number is represented as a double with the 3 least significant bits of the mantissa set to 0.

#### 2.1.2 Boolean Value ###

A boolean BonValue can be thought of as casting a BonValue\* to the following BoolValue\*:

~~~
struct BoolValue {
	int32_t			type;		/* BON_VT_BOOL */
	int32_t			boolValue;	/* 0 == false, 1 == true */
};
~~~

#### 2.1.3 String Value ###

~~~
struct StringValue {
	int32_t			type;		/* BON_VT_STRING */
	int32_t			offset;		/* offset relative &stringValue where the string
						 * is located in the record. */
} stringValue;
~~~

#### 2.1.4 Object Value ###

~~~
struct ObjectValue {
	int32_t			type;		/* BON_VT_OBJECT */
	int32_t			offset;		/* offset relative &objectValue where the object
						 * is located in the record. */
} objectValue;
~~~

#### 2.1.5 Array Value ###

~~~
struct ArrayValue {
	int32_t			type;		/* BON_VT_ARRAY */
	int32_t			offset;		/* offset relative &arrayValue where the array
						 * is located in the record. */
} arrayValue;
~~~

#### 2.1.6 Null Value ###

Null is represented as (uint64\_t)7


### 2.1 Header ###

The header is a binary representation of a BonRecord struct in little endian format.

~~~
typedef struct BonRecord {
	uint32_t		magic;			/* FourCC('B', 'O', 'N', ' ') */
	uint32_t		reserved;		/* Must be 0 */
	uint32_t		recordSize;		/* Total size of the entire record */
	int32_t			nameLookupTableOffset;	/* Offset to name lookup table relative &nameLookupTableOffset */
	BonValue		rootValue;		/* Variant referencing the root value in the record (an array or an object) */
} BonRecord;
~~~

### 2.2 Arrays ###

### 2.3 Objects ###

### 2.4 Value Strings ###

### 2.5 Name to String Lookup Table ###

### 2.6 Name Strings ###




