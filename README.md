BON - Binary Object Notation
============================

Abstract
--------

BON is an binary and complete representation of JSON with the following potentially interesting
features:
- A BON record is memory mappable and mem moveable. No parsing or fixup required.
- A BON record is canonical in the sense that two semantically identical JSON documents are
  represented by identical BON records. 
- Homogenous arrays of numbers can be accessed as a simple array of doubles.
- Object members are sorted by their name's 32-bit murmur3 hashes. This makes linear parsing of an
  object's members possible. Searching for a key is log2(N), too.

BON is not a serialization format like BSON.

Introduction
---------------

JSON is interesting and useful as an interchange format due to its simplicity and widespread use.
Especially when considering doing anything in a browser with javascript.

JSON is also a text based format which is not at all good for machine reading. The parsing portion
is a big and expensive part when working with large JSON files.

The idea behind BON is to provide is a binary representation of a JSON file which can be used without any parsing
at all.

What this project provides is:
- A specification of the BON record format.
- A C implementation for reading a BON record (src/Bon.h & src/Bon.c)
- A C implementation for converting from JSON to a BON record and vice versa (src/BonConvert.h & src/BonConvert.c)
- A C implementation for editing BON records without falling back to JSON. (src/Beon.h & src/Beon.c)

Here are some ideas where this could be useful:
- Caching parsed JSON files as BON records for faster loads.
- Using BON records for storing and working with data and falling back to JSON only when it is
  needed for interop.
- Using BON records instead of JSON as runtime data as a faster alternative to JSON (for simpler
  games, for example).

Building
--------------

### OS X ###

1. Get the tundra build system from https://github.com/deplinenoise/tundra 
2. Run 'tundra' from the root directory. Should work on any other unix-like, little-endian platform, too.
   - This will build the library and all tools and test code.
3. Use the output from tundra-output/<config>

### Windows ###

Use bon.sln (*Not up to date at the moment!*)

### Documentation ###

1. Run 'doxygen doc/bon.doxygen' from the root folder.
2. Launch the html documentation from html/index.html

Using
--------------

### Working with BON records from the command line? ###

There are a few tools that are provided with this distribution that are useful for working with
BON records from the command line.

- Json2Bon : Convert a JSON text to a BON record.
- Bon2Json : Convert a BON record to JSON text.
- DumpBon : Debug tool for printing the contents of a BON record.

### Just interested in reading existing BON records? ###

Include the files:
- Bon.h and 
- Bon.c

in your project and use the API in Bon.h.

### Interested in transforming BON records from/to JSON? ###

Include the files:
- Bon.h, 
- Bon.c, 
- BonConvert.h and
- BonConvert.c 

in your project and use the API in Bon.h and BonConvert.h.

BON Format
--------------

A BON record consists of the following sections (in this order):
1. Header
2. Objects
3. Arrays
4. Value strings
5. Name to string lookup table
6. Name strings

A BON record is stored in little-endian format. Support for big-endian could easily be added by
checking the magic value in the header.

A JSON text is converted and canonicalized into a BON record in the following way:
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
- Name and value strings are ordered by their hash (ascending order).
- There may be an identical name string and value string, though. The reason for that is that the
  last two sections are not necessary to understand or use a record, so dropping them could be a
  space saving alternative for some applications.
- Numbers are normalized (todo)

### Values ###

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

The rationale for using a full 64-bit value for all these types is partly for simplicity when
iterating over an array of items and partly because the in-place editing of a BON record sometimes
need to store a pointer in the same slot as a string, object or array.

A 32-bit value would be enough for all types except a number (double). Using floats for numbers
would be too limiting since we would sacrifice 3 bits of the mantissa for the type.

#### Number Value ###

A number is represented as a double with the 3 least significant bits of the mantissa set to 0.

#### Boolean Value ###

A boolean BonValue should be interpreted as:

~~~
typedef struct BonBoolValue {
	int32_t			type;			/**< BON_VT_BOOL **/
	BonBool			value;			/**< BON_FALSE or BON_TRUE */
} BonBoolValue;
~~~

#### String Value ###

A string BonValue should be interpreted as:

~~~
typedef struct BonStringValue {
	int32_t			type;			/**< BON_VT_STRING **/
	int32_t			offset;			/**< Address of this struct + offset points to the string */
} BonStringValue;
~~~

#### Object Value ###

An object BonValue should be interpreted as:

~~~
typedef struct BonObjectValue {
	int32_t			type;			/**< BON_VT_OBJECT **/
	int32_t			offset;			/**< Address of this struct + offset points to the object */
} BonObjectValue;
~~~

#### Array Value ###

An array BonValue should be interpreted as:
~~~
typedef struct BonArrayValue {
	int32_t			type;			/**< BON_VT_ARRAY **/
	int32_t			offset;			/**< Address of this struct + offset points to the array */
} BonArrayValue;
~~~

#### Null Value ###

Null is represented as (uint64\_t)7


### Header ###

The header is a binary representation of a BonRecord struct in little endian format.

~~~
typedef struct BonRecord {
	uint32_t		magic;			/**< FourCC('B', 'O', 'N', ' ') */
	uint32_t		recordSize;		/**< Total size of the entire record */
	uint32_t		reserved;		/**< Must be 0 */
	uint32_t		reserved1;		/**< Must be 0 */
	int32_t			valueStringOffset;	/**< Offset to first value string &valueStringOffset */
	int32_t			nameLookupTableOffset;	/**< Offset to name lookup table relative &nameLookupTableOffset */
	BonValue		rootValue;		/**< Variant referencing the root value in the record (an array or an object) */
} BonRecord;
~~~

### Objects ###

The first object (or array if there are no objects in the record) are stored directly after the
header.

The first eight bytes of an object is a container header:

~~~
typedef struct BonContainerHeader {
	int32_t			capacity;		/**< Container capacity */
	int32_t			count;			/**< Number of items in the container */
} BonContainerHeader;
~~~

An object capacity is always negative. I.e. -5 means the capacity is 5.

Abs(capacity) is always equal to count in a BON record stored on, say, disk. 

The rationale for having space for a capacity there at all is for in-place editing purposes.

After the header follows count number of BonValues.

After that follows an array of count BonName which is the hash values of the object's member keys.
I.e. BonValue at index 4 has a name in the BonName array at index 4.

The BonNames are stored in ascending order which makes binary searching easy. It is also possible
to parse the object linearly if the set of hashes are known by the application.

The array of BonNames is padded with zeros up to an eight byte boundary.

### Arrays ###

Arrays are stored identically to an object. The difference is that the capacity is positive and
that there is no BonName array.

### Value Strings ###

In the value strings section all string type values are stored as null-terminated strings,
starting at eight byte boundaries.

I.e. the string "purposes" would occupy 16 bytes since the length including null is 9 and it is
padded to the nearest 8 byte boundary.

### Name to String Lookup Table ###

If the actual strings of the object members's keys are needed, they can be looked up in this
table.

It starts with a container header:

~~~
typedef struct BonContainerHeader {
	int32_t			capacity;		/**< Container capacity */
	int32_t			count;			/**< Number of items in the container */
} BonContainerHeader;
~~~

And is followed by count BonNameAndOffset entries ordered by ascending name.

~~~
typedef struct BonNameAndOffset {
	BonName			name;			/**< The hash of a object member's name. */
	int32_t			offset;			/**< Address of offset + offset points to the name's string. */
} BonNameAndOffset;
~~~

### Name Strings ###

Name strings are stored in the same way as value strings.


