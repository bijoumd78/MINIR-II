#ifndef _MRGLOBALDEFINITIONS_MRBASICTYPES_H
#define _MRGLOBALDEFINITIONS_MRBASICTYPES_H

/*---------------------------------------------------------------------------*/
/*  Copyright (C) Siemens AG 2004  All Rights Reserved.  Confidential        */
/*---------------------------------------------------------------------------*/
/*
 * Project: NUMARIS/4
 *    File: \n4_common1\pkg\MrCommon\MrGlobalDefinitions\MrBasicTypes.h
 * Version:
 *  Author: Comp_SWArT KOELRIRG
 *    Date: n.a.
 *
 *    Lang: C / C++
 *
 * Descrip: Generic data types, compatible for all compilers.
 *          
 *---------------------------------------------------------------------------*/


/** \file
\brief This file contains portable data types, intended for use with various compilers,
operating systems and target architectures.

These types are modeled after the C99 standard draft and will most likely later
be replaced by the <stdint.h> include, as soon as they are available.

Note: This documentation is gegerated for the Microsoft Compiler and MS Windows.
The definitions for other compilers and target systems may vary.

\section Subpages

- \ref EIT "Exact-Width Integer Types"
- \ref MIT "Minimum-Width Integer Types"
- \ref FIT "Fastest Minimum-Width Integer Types"
- \ref IP  "Integer Types Capable of Holding Object Pointers"
- \ref GWI "Greatest-Width Integer Types"
- \ref MACRO "Portable Macros for Defining Constants of Various Sizes"

\section BASICTYPES_LIB Libraries
none.

*/

/** 
\defgroup EIT Exact-Width Integer Types

These types are made following the pattern \c intX_t, where X = 8, 16, 32 and 64. 
Use these types if you need exactly the number of bits specified.

In theory, there migth be a chance that some target architectures do not
support all data types (for example, there are no 24-bit types here).
You may therefore want to use the data types defined below whenever possible.

@{
*/
#if defined(_MSC_VER)
/* Microsoft Compiler */
    typedef signed char         int8_t;    ///< Signed integer having exactly 8 bits
    typedef short               int16_t;   ///< Signed integer having exactly 16 bits
    typedef long                int32_t;   ///< Signed integer having exactly 32 bits
    typedef __int64             int64_t;   ///< Signed integer having exactly 64 bits
    typedef unsigned char       uint8_t;   ///< Unsigned integer having exactly 8 bits
    typedef unsigned short      uint16_t;  ///< Unsigned integer having exactly 16 bits
    typedef unsigned long       uint32_t;  ///< Unsigned integer having exactly 32 bits
    typedef unsigned __int64    uint64_t;  ///< Unsigned integer having exactly 64 bits
#elif defined (BUILD_PLATFORM_LINUX)
/* Linux already supports the C99 types */
    #ifndef __KERNEL__
        #include <inttypes.h>
    #else
        #include <linux/types.h>
    #endif    
#elif defined (VXWORKS)
/* vxWorks already has a build-in header file for basic ANSI types */
    #include <types/vxTypes.h>
#elif defined (__GNUC__)
/* GNU compiler */
    typedef signed char             int8_t;
    typedef short int               int16_t;
    typedef int                     int32_t;
  #if defined (__x86_64__)
    typedef long int                int64_t;
  #else
    typedef long long int           int64_t;
  #endif
    typedef unsigned char           uint8_t;
    typedef unsigned short int      uint16_t;
    typedef unsigned int            uint32_t;
  #if defined (__x86_64__)
    typedef unsigned long int       uint64_t;
  #else
    typedef unsigned long long int  uint64_t;
  #endif
#elif defined (_INTEL_COMPILER)
/* Intel Compiler */
    typedef signed char         int8_t;
    typedef short               int16_t;
    typedef long                int32_t;
    typedef __int64             int64_t;
    typedef unsigned char       uint8_t;
    typedef unsigned short      uint16_t;
    typedef unsigned long       uint32_t;
    typedef unsigned __int64    uint64_t;  
#elif  defined (DSP_0) || defined (DSP_1)
    typedef signed char         int8_t;  
    typedef short               int16_t; 
    typedef long                int32_t; 
    typedef unsigned char       uint8_t; 
    typedef unsigned short      uint16_t;
    typedef unsigned long       uint32_t; 
    typedef unsigned long long int uint64_t;
    typedef long long int       int64_t;
#else
    #error Undefined basic types for this platform
#endif

/** @} */


/** 
\defgroup MIT Minimum-Width Integer Types
These types are made following the pattern \c int_leastX_t, where X = 8, 16, 24, 32 and 64. 
Use these types if you need at least the number of bits specified.

Depending on the target system architecture, a data type may have more bits than needed,
but a larger data type will only be choosen if no smaller is available.

@{
*/

#if defined (BUILD_PLATFORM_LINUX)
    // Linux has everything except 24-bit
    typedef int32_t  int_least24_t;   ///< Signed integer having at least 24 bits
    typedef uint32_t uint_least24_t;  ///< Unsigned integer having at least 24 bits
#elif defined (VXWORKS) 
    typedef int8_t   int_least8_t;    ///< Signed integer having at least 8 bits
    typedef int16_t  int_least16_t;   ///< Signed integer having at least 16 bits
    typedef int32_t  int_least24_t;   ///< Signed integer having at least 24 bits
    typedef int32_t  int_least32_t;   ///< Signed integer having at least 32 bits
    typedef uint8_t  uint_least8_t;   ///< Unsigned integer having at least 8 bits
    typedef uint16_t uint_least16_t;  ///< Unsigned integer having at least 16 bits
    typedef uint32_t uint_least24_t;  ///< Unsigned integer having at least 24 bits
    typedef uint32_t uint_least32_t;  ///< Unsigned integer having at least 32 bits
#else
    typedef int8_t   int_least8_t;    ///< Signed integer having at least 8 bits
    typedef int16_t  int_least16_t;   ///< Signed integer having at least 16 bits
    typedef int32_t  int_least24_t;   ///< Signed integer having at least 24 bits
    typedef int32_t  int_least32_t;   ///< Signed integer having at least 32 bits
    typedef int64_t  int_least64_t;   ///< Signed integer having at least 64 bits
    typedef uint8_t  uint_least8_t;   ///< Unsigned integer having at least 8 bits
    typedef uint16_t uint_least16_t;  ///< Unsigned integer having at least 16 bits
    typedef uint32_t uint_least24_t;  ///< Unsigned integer having at least 24 bits
    typedef uint32_t uint_least32_t;  ///< Unsigned integer having at least 32 bits
    typedef uint64_t uint_least64_t;  ///< Unsigned integer having at least 64 bits
#endif

/** @} */


/**
\defgroup FIT Fastest Minimum-Width Integer Types
These types are made following the pattern \c int_fastX_t, where X = 8, 16, 24, 32 and 64. 
Use these types if you need at least the number of bits specified, but allow the compiler
to choose a representation with optimum performance.

Depending on the target system architecture, a data type may have more bits than needed,
and it will frequently be done e.g. to optimize memory alignment.

@{
*/

#if defined (BUILD_PLATFORM_LINUX)
    // Linux has everything except 24-bit
    typedef int64_t  int_fast24_t;   ///< Signed integer having at least 24 bits, speed optimized
    typedef uint64_t uint_fast24_t;  ///< Unsigned integer having at least 24 bits, speed optimized
#elif defined (__GNUC__) && defined (__x86_64__)
    typedef int8_t   int_fast8_t;    ///< Signed integer having at least 8 bits, speed optimized
    typedef int64_t  int_fast16_t;   ///< Signed integer having at least 16 bits, speed optimized
    typedef int64_t  int_fast24_t;   ///< Signed integer having at least 24 bits, speed optimized
    typedef int64_t  int_fast32_t;   ///< Signed integer having at least 32 bits, speed optimized
    typedef int64_t  int_fast64_t;   ///< Signed integer having at least 64 bits, speed optimized
    typedef uint8_t  uint_fast8_t;   ///< Unsigned integer having at least 8 bits, speed optimized
    typedef uint64_t uint_fast16_t;  ///< Unsigned integer having at least 16 bits, speed optimized
    typedef uint64_t uint_fast24_t;  ///< Unsigned integer having at least 24 bits, speed optimized
    typedef uint64_t uint_fast32_t;  ///< Unsigned integer having at least 32 bits, speed optimized
    typedef uint64_t uint_fast64_t;  ///< Unsigned integer having at least 64 bits, speed optimized
#elif defined (VXWORKS) 
    typedef int8_t   int_fast8_t;    ///< Signed integer having at least 8 bits, speed optimized
    typedef int16_t  int_fast16_t;   ///< Signed integer having at least 16 bits, speed optimized
    typedef int32_t  int_fast24_t;   ///< Signed integer having at least 24 bits, speed optimized
    typedef int32_t  int_fast32_t;   ///< Signed integer having at least 32 bits, speed optimized
    typedef uint8_t  uint_fast8_t;   ///< Unsigned integer having at least 8 bits, speed optimized
    typedef uint16_t uint_fast16_t;  ///< Unsigned integer having at least 16 bits, speed optimized
    typedef uint32_t uint_fast24_t;  ///< Unsigned integer having at least 24 bits, speed optimized
    typedef uint32_t uint_fast32_t;  ///< Unsigned integer having at least 32 bits, speed optimized
#else
    typedef int8_t   int_fast8_t;    ///< Signed integer having at least 8 bits, speed optimized
    typedef int16_t  int_fast16_t;   ///< Signed integer having at least 16 bits, speed optimized
    typedef int32_t  int_fast24_t;   ///< Signed integer having at least 24 bits, speed optimized
    typedef int32_t  int_fast32_t;   ///< Signed integer having at least 32 bits, speed optimized
    typedef int64_t  int_fast64_t;   ///< Signed integer having at least 64 bits, speed optimized
    typedef uint8_t  uint_fast8_t;   ///< Unsigned integer having at least 8 bits, speed optimized
    typedef uint16_t uint_fast16_t;  ///< Unsigned integer having at least 16 bits, speed optimized
    typedef uint32_t uint_fast24_t;  ///< Unsigned integer having at least 24 bits, speed optimized
    typedef uint32_t uint_fast32_t;  ///< Unsigned integer having at least 32 bits, speed optimized
    typedef uint64_t uint_fast64_t;  ///< Unsigned integer having at least 64 bits, speed optimized
#endif

/** @} */

/**
\defgroup IP Integer Types Capable of Holding Object Pointers

Use these types if you need to store a pointer in an integer. 

Note: The size of this type may depend on the system architecture

@{
*/

#if defined (BUILD_PLATFORM_LINUX)
   // Linux has them already
#elif defined(__WIN64) || defined(__LP64__)
/* For 64-bit architecture */
   typedef int64_t  intptr_t;       ///< Signed integer type capable of holding a pointer
   typedef uint64_t uintptr_t;      ///< Unsigned integer type capable of holding a pointer
#else
/* For 32-bit architecture */
//   typedef int32_t  intptr_t;      ///< Signed integer type capable of holding a pointer
//   typedef uint32_t uintptr_t;     ///< Unsigned integer type capable of holding a pointer
#endif

/** @} */

/**
\defgroup GWI Greatest-Width Integer Types

These types are capable to hold any value of any (signed or unsigned) integer type

Note: The size of this type may depend on the system architecture

@{
*/

#if defined (BUILD_PLATFORM_LINUX)
   // Linux has them already
#elif defined (VXWORKS) 
   // is this right ???
   typedef int32_t  intmax_t;       ///< Type capable of holding signed integers of \em any size
   typedef uint32_t uintmax_t;      ///< Type capable of holding unsigned integers of \em any size   
#else
   typedef int64_t  intmax_t;       ///< Type capable of holding signed integers of \em any size
   typedef uint64_t uintmax_t;      ///< Type capable of holding unsigned integers of \em any size
#endif

/** @} */

/**
\defgroup MACRO Portable Macros for Defining Constants of Various Sizes

These definitions are made following the pattern \c INTX_C and \c UINTX_C, where X = 8, 16, 24, 32 and 64.

Note that the result type is the "least" type, since it is not guaranteed that the "exact"
type exists for all platforms.

@{
*/

#if defined(_MSC_VER)
/* Microsoft Compiler */
    /// Macro for defining an signed integer of at least 8 bit
    #define INT8_C(x)   int_least8_t(x)
    /// Macro for defining an signed integer of at least 16 bit
    #define INT16_C(x)  int_least16_t(x)
    /// Macro for defining an signed integer of at least 24 bit
    #define INT24_C(x)  int_least24_t(x##L)
    /// Macro for defining an signed integer of at least 32 bit
    #define INT32_C(x)  int_least32_t(x##L)
    /// Macro for defining an signed integer of at least 64 bit
    #define INT64_C(x)  int_least64_t(x##i64);
    /// Macro for defining an unsigned integer of at least 8 bit
    #define UINT8_C(x)  uint_least8_t(x)
    /// Macro for defining an unsigned integer of at least 16 bit
    #define UINT16_C(x) uint_least16_t(x)
    /// Macro for defining an unsigned integer of at least 24 bit
    #define UINT24_C(x) uint_least24_t(x##UL)
    /// Macro for defining an unsigned integer of at least 32 bit
    #define UINT32_C(x) uint_least32_t(x##UL)
    /// Macro for defining an unsigned integer of at least 64 bit
    #define UINT64_C(x) uint_least64_t(x##ui64);
#elif defined (__GNUC__)
/* GNU Compiler */
    #define INT8_C(x)   int_least8_t(x)
    #define INT16_C(x)  int_least16_t(x)
    #define INT24_C(x)  int_least24_t(x)
    #define INT32_C(x)  int_least32_t(x)
  #if defined (__x86_64__)
    #define INT64_C(x)  int_least64_t(x##L);
  #else
    #define INT64_C(x)  int_least64_t(x##LL);
  #endif
    #define UINT8_C(x)  uint_least8_t(x)
    #define UINT16_C(x) uint_least16_t(x)
    #define UINT24_C(x) uint_least32_t(x)
    #define UINT32_C(x) uint_least32_t(x)
  #if defined (__x86_64__)
    #define UINT64_C(x) uint_least64_t(x##UL);
  #else
    #define UINT64_C(x) uint_least64_t(x##ULL);
  #endif
#elif defined (_INTEL_COMPILER)
/* Intel Compiler */
    #define INT8_C(x)   int_least8_t(x)
    #define INT16_C(x)  int_least16_t(x)
    #define INT24_C(x)  int_least24_t(x##L)
    #define INT32_C(x)  int_least32_t(x##L)
    #define INT64_C(x)  int_least64_t(x##i64);
    #define UINT8_C(x)  uint_least8_t(x)
    #define UINT16_C(x) uint_least16_t(x)
    #define UINT24_C(x) uint_least32_t(x##UL)
    #define UINT32_C(x) uint_least32_t(x##UL)
    #define UINT64_C(x) uint_least64_t(x##ui64);
#endif
/** @} */


#ifdef __cplusplus

/**
An array template intended for interfaces where it is important that there are no gaps between array elements
*/

    template<typename T, intmax_t N>
    class packed_array
    {
    private:
#if defined(_MSC_VER) || defined(_INTEL_COMPILER)
    #pragma pack(push,1)
        T m_Value[N];
    #pragma pack(pop)
#elif defined(__GNUC__)
        T m_Value[N];
#else
    #error Unknown compiler. Insert appropriate pack-attribute here.
#endif
    public:
        /// Returns the address of the array (first element)
        operator T* () { return m_Value; }
    };



/**
This template is needed for interfaces close to the hardware, where complex values are packed,
not aligned to any word or dword boundaries. Members are public and are accessed directly.
*/
    template<typename T>
    struct packed_complex
    {
#if defined(_MSC_VER) || defined(_INTEL_COMPILER)
    #pragma pack(push,1)
        T m_re, m_im;
    #pragma pack(pop)
#elif defined(__GNUC__)
        T m_re;
        T m_im;
#else
    #error Unknown compiler. Insert appropriate pack-attribute here.
#endif
  };

#endif

#endif
