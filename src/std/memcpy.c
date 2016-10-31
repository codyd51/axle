/********************************************************************
 ** File:     memcpy.c
 **
 ** Copyright (C) 1999-2010 Daniel Vik
 **
 ** This software is provided 'as-is', without any express or implied
 ** warranty. In no event will the authors be held liable for any
 ** damages arising from the use of this software.
 ** Permission is granted to anyone to use this software for any
 ** purpose, including commercial applications, and to alter it and
 ** redistribute it freely, subject to the following restrictions:
 **
 ** 1. The origin of this software must not be misrepresented; you
 **    must not claim that you wrote the original software. If you
 **    use this software in a product, an acknowledgment in the
 **    use this software in a product, an acknowledgment in the
 **    product documentation would be appreciated but is not
 **    required.
 **
 ** 2. Altered source versions must be plainly marked as such, and
 **    must not be misrepresented as being the original software.
 **
 ** 3. This notice may not be removed or altered from any source
 **    distribution.
 **
 **
 ** Description: Implementation of the standard library function memcpy.
 **             This implementation of memcpy() is ANSI-C89 compatible.
 **
 **             The following configuration options can be set:
 **
 **           LITTLE_ENDIAN   - Uses processor with little endian
 **                             addressing. Default is big endian.
 **
 **           PRE_INC_PTRS    - Use pre increment of pointers.
 **                             Default is post increment of
 **                             pointers.
 **
 **           INDEXED_COPY    - Copying data using array indexing.
 **                             Using this option, disables the
 **                             PRE_INC_PTRS option.
 **
 **           MEMCPY_64BIT    - Compiles memcpy for 64 bit
 **                             architectures
 **
 **
 ** Best Settings:
 **
 ** Intel x86:  LITTLE_ENDIAN and INDEXED_COPY
 **
 *******************************************************************/



/********************************************************************
 ** Configuration definitions.
 *******************************************************************/

#define LITTLE_ENDIAN
#define INDEXED_COPY


/********************************************************************
 ** Includes for size_t definition
 *******************************************************************/

#include <stddef.h>


/********************************************************************
 ** Typedefs
 *******************************************************************/

typedef unsigned char       UInt8;
typedef unsigned short      UInt16;
typedef unsigned int        UInt32;
#ifdef _WIN32
typedef unsigned __int64    UInt64;
#else
typedef unsigned long long  UInt64;
#endif

#ifdef MEMCPY_64BIT
typedef UInt64              UIntN;
#define TYPE_WIDTH          8L
#else
typedef UInt32              UIntN;
#define TYPE_WIDTH          4L
#endif


/********************************************************************
 ** Remove definitions when INDEXED_COPY is defined.
 *******************************************************************/

#if defined (INDEXED_COPY)
#if defined (PRE_INC_PTRS)
#undef PRE_INC_PTRS
#endif /*PRE_INC_PTRS*/
#endif /*INDEXED_COPY*/



/********************************************************************
 ** Definitions for pre and post increment of pointers.
 *******************************************************************/

#if defined (PRE_INC_PTRS)

#define START_VAL(x)            (x)--
#define INC_VAL(x)              *++(x)
#define CAST_TO_U8(p, o)        ((UInt8*)p + o + TYPE_WIDTH)
#define WHILE_DEST_BREAK        (TYPE_WIDTH - 1)
#define PRE_LOOP_ADJUST         - (TYPE_WIDTH - 1)
#define PRE_SWITCH_ADJUST       + 1

#else /*PRE_INC_PTRS*/

#define START_VAL(x)
#define INC_VAL(x)              *(x)++
#define CAST_TO_U8(p, o)        ((UInt8*)p + o)
#define WHILE_DEST_BREAK        0
#define PRE_LOOP_ADJUST
#define PRE_SWITCH_ADJUST

#endif /*PRE_INC_PTRS*/



/********************************************************************
 ** Definitions for endians
 *******************************************************************/

#if defined (LITTLE_ENDIAN)

#define SHL >>
#define SHR <<

#else /* LITTLE_ENDIAN */

#define SHL <<
#define SHR >>

#endif /* LITTLE_ENDIAN */



/********************************************************************
 ** Macros for copying words of  different alignment.
 ** Uses incremening pointers.
 *******************************************************************/

#define CP_INCR() {                         \
    INC_VAL(dstN) = INC_VAL(srcN);          \
}

#define CP_INCR_SH(shl, shr) {              \
    dstWord   = srcWord SHL shl;            \
    srcWord   = INC_VAL(srcN);              \
    dstWord  |= srcWord SHR shr;            \
    INC_VAL(dstN) = dstWord;                \
}



/********************************************************************
 ** Macros for copying words of  different alignment.
 ** Uses array indexes.
 *******************************************************************/

#define CP_INDEX(idx) {                     \
    dstN[idx] = srcN[idx];                  \
}

#define CP_INDEX_SH(x, shl, shr) {          \
    dstWord   = srcWord SHL shl;            \
    srcWord   = srcN[x];                    \
    dstWord  |= srcWord SHR shr;            \
    dstN[x]  = dstWord;                     \
}



/********************************************************************
 ** Macros for copying words of different alignment.
 ** Uses incremening pointers or array indexes depending on
 ** configuration.
 *******************************************************************/

#if defined (INDEXED_COPY)

#define CP(idx)               CP_INDEX(idx)
#define CP_SH(idx, shl, shr)  CP_INDEX_SH(idx, shl, shr)

#define INC_INDEX(p, o)       ((p) += (o))

#else /* INDEXED_COPY */

#define CP(idx)               CP_INCR()
#define CP_SH(idx, shl, shr)  CP_INCR_SH(shl, shr)

#define INC_INDEX(p, o)

#endif /* INDEXED_COPY */


#define COPY_REMAINING(count) {                                     \
    START_VAL(dst8);                                                \
    START_VAL(src8);                                                \
                                                                    \
    switch (count) {                                                \
    case 7: INC_VAL(dst8) = INC_VAL(src8);                          \
    case 6: INC_VAL(dst8) = INC_VAL(src8);                          \
    case 5: INC_VAL(dst8) = INC_VAL(src8);                          \
    case 4: INC_VAL(dst8) = INC_VAL(src8);                          \
    case 3: INC_VAL(dst8) = INC_VAL(src8);                          \
    case 2: INC_VAL(dst8) = INC_VAL(src8);                          \
    case 1: INC_VAL(dst8) = INC_VAL(src8);                          \
    case 0:                                                         \
    default: break;                                                 \
    }                                                               \
}

#define COPY_NO_SHIFT() {                                           \
    UIntN* dstN = (UIntN*)(dst8 PRE_LOOP_ADJUST);                   \
    UIntN* srcN = (UIntN*)(src8 PRE_LOOP_ADJUST);                   \
    size_t length = count / TYPE_WIDTH;                             \
                                                                    \
    while (length & 7) {                                            \
        CP_INCR();                                                  \
        length--;                                                   \
    }                                                               \
                                                                    \
    length /= 8;                                                    \
                                                                    \
    while (length--) {                                              \
        CP(0);                                                      \
        CP(1);                                                      \
        CP(2);                                                      \
        CP(3);                                                      \
        CP(4);                                                      \
        CP(5);                                                      \
        CP(6);                                                      \
        CP(7);                                                      \
                                                                    \
        INC_INDEX(dstN, 8);                                         \
        INC_INDEX(srcN, 8);                                         \
    }                                                               \
                                                                    \
    src8 = CAST_TO_U8(srcN, 0);                                     \
    dst8 = CAST_TO_U8(dstN, 0);                                     \
                                                                    \
    COPY_REMAINING(count & (TYPE_WIDTH - 1));                       \
                                                                    \
    return dest;                                                    \
}



#define COPY_SHIFT(shift) {                                         \
    UIntN* dstN  = (UIntN*)((((UIntN)dst8) PRE_LOOP_ADJUST) &       \
                             ~(TYPE_WIDTH - 1));                    \
    UIntN* srcN  = (UIntN*)((((UIntN)src8) PRE_LOOP_ADJUST) &       \
                             ~(TYPE_WIDTH - 1));                    \
    size_t length  = count / TYPE_WIDTH;                            \
    UIntN srcWord = INC_VAL(srcN);                                  \
    UIntN dstWord;                                                  \
                                                                    \
    while (length & 7) {                                            \
        CP_INCR_SH(8 * shift, 8 * (TYPE_WIDTH - shift));            \
        length--;                                                   \
    }                                                               \
                                                                    \
    length /= 8;                                                    \
                                                                    \
    while (length--) {                                              \
        CP_SH(0, 8 * shift, 8 * (TYPE_WIDTH - shift));              \
        CP_SH(1, 8 * shift, 8 * (TYPE_WIDTH - shift));              \
        CP_SH(2, 8 * shift, 8 * (TYPE_WIDTH - shift));              \
        CP_SH(3, 8 * shift, 8 * (TYPE_WIDTH - shift));              \
        CP_SH(4, 8 * shift, 8 * (TYPE_WIDTH - shift));              \
        CP_SH(5, 8 * shift, 8 * (TYPE_WIDTH - shift));              \
        CP_SH(6, 8 * shift, 8 * (TYPE_WIDTH - shift));              \
        CP_SH(7, 8 * shift, 8 * (TYPE_WIDTH - shift));              \
                                                                    \
        INC_INDEX(dstN, 8);                                         \
        INC_INDEX(srcN, 8);                                         \
    }                                                               \
                                                                    \
    src8 = CAST_TO_U8(srcN, (shift - TYPE_WIDTH));                  \
    dst8 = CAST_TO_U8(dstN, 0);                                     \
                                                                    \
    COPY_REMAINING(count & (TYPE_WIDTH - 1));                       \
                                                                    \
    return dest;                                                    \
}


/********************************************************************
 **
 ** void *memcpy(void *dest, const void *src, size_t count)
 **
 ** Args:     dest        - pointer to destination buffer
 **           src         - pointer to source buffer
 **           count       - number of bytes to copy
 **
 ** Return:   A pointer to destination buffer
 **
 ** Purpose:  Copies count bytes from src to dest.
 **           No overlap check is performed.
 **
 *******************************************************************/

void *memcpy(void *dest, const void *src, size_t count)
{
    UInt8* dst8 = (UInt8*)dest;
    UInt8* src8 = (UInt8*)src;

    if (count < 8) {
        COPY_REMAINING(count);
        return dest;
    }

    START_VAL(dst8);
    START_VAL(src8);

    while (((UIntN)dst8 & (TYPE_WIDTH - 1)) != WHILE_DEST_BREAK) {
        INC_VAL(dst8) = INC_VAL(src8);
        count--;
    }

    switch ((((UIntN)src8) PRE_SWITCH_ADJUST) & (TYPE_WIDTH - 1)) {
    case 0: COPY_NO_SHIFT(); break;
    case 1: COPY_SHIFT(1);   break;
    case 2: COPY_SHIFT(2);   break;
    case 3: COPY_SHIFT(3);   break;
#if TYPE_WIDTH > 4
    case 4: COPY_SHIFT(4);   break;
    case 5: COPY_SHIFT(5);   break;
    case 6: COPY_SHIFT(6);   break;
    case 7: COPY_SHIFT(7);   break;
#endif
    }
	return dest;
}
