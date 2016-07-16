#ifndef DENIS_META_H_
#define DENIS_META_H_

#include "windows.h"
#undef max

#include "stdint.h"

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

#define MAX(val1, val2) ((val1) > (val2) ? (val1) : (val2))
#define SWAP_DATA(a, b, type)			\
    do {						\
     type temp = a;				\
     a = b;					\
     b = temp;					\
    } while(0)

//NOTE(denis): assumes the char array is a valid string
static inline bool charInArray(char c, char array[])
{
    bool result = false;

    if (array != 0)
    {
	for (int i = 0; array[i] != 0 && !result; ++i)
	{
	    if (array[i] == c)
		result = true;
	}
    }
    
    return result;
}

//NOTE(denis): only works for valid strings
static inline bool stringsEqual(char *A, char *B)
{
    bool result = true;

    int charIndex = 0;
    while (A[charIndex] != 0 && B[charIndex] != 0 && result)
    {
	if (A[charIndex] != B[charIndex])
	    result = false;
	
	++charIndex;
    }

    if (A[charIndex] != B[charIndex])
	result = false;
    
    return result;
}

static inline uint8* growArray(void *array, int arrayLength, int typeSize, int newLength)
{
    uint8 *newArray = (uint8*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
					typeSize*newLength);
    
    if (newArray && array != 0 && arrayLength > 0)
    {
	for (int i = 0; i < arrayLength*typeSize/sizeof(uint8); ++i)
	{
	    *(newArray + i) = *((uint8*)array + i);
	}

	HeapFree(GetProcessHeap(), 0, array);
    }
    
    return newArray;
}

#endif