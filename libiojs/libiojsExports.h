/*
 * Summary: macros for marking symbols as exportable/importable.
 * Description: macros for marking symbols as exportable/importable.
 *
 * Copy: See Copyright for the status of this software.
 *
 * Author: Igor Zlatkovic <igor@zlatkovic.com>
 */

#ifndef __LIBIOJS_EXPORTS_H__
#define __LIBIOJS_EXPORTS_H__

/**
 * LIBIOJSPUBFUN:
 * LIBIOJSPUBFUN, LIBIOJSPUBVAR, LIBIOJSCALL
 *
 * Macros which declare an exportable function, an exportable variable and
 * the calling convention used for functions.
 *
 * Please use an extra block for every platform/compiler combination when
 * modifying this, rather than overlong #ifdef lines. This helps
 * readability as well as the fact that different compilers on the same
 * platform might need different definitions.
 */

/**
 * LIBIOJSPUBFUN:
 *
 * Macros which declare an exportable function
 */
#define LIBIOJSPUBFUN
/**
 * LIBIOJSPUBVAR:
 *
 * Macros which declare an exportable variable
 */
#define LIBIOJSPUBVAR extern
/**
 * LIBIOJSCALL:
 *
 * Macros which declare the called convention for exported functions
 */
#define LIBIOJSCALL

/** DOC_DISABLE */

/* Windows platform with MS compiler */
#if defined(_WIN32) && defined(_MSC_VER)
  #undef LIBIOJSPUBFUN
  #undef LIBIOJSPUBVAR
  #undef LIBIOJSCALL
  #if defined(IN_LIBLIBIOJS) && !defined(LIBLIBIOJS_STATIC)
    #define LIBIOJSPUBFUN __declspec(dllexport)
    #define LIBIOJSPUBVAR __declspec(dllexport)
  #else
    #define LIBIOJSPUBFUN
    #if !defined(LIBLIBIOJS_STATIC)
      #define LIBIOJSPUBVAR __declspec(dllimport) extern
    #else
      #define LIBIOJSPUBVAR extern
    #endif
  #endif
  #define LIBIOJSCALL __cdecl
  #if !defined _REENTRANT
    #define _REENTRANT
  #endif
#endif

/* Windows platform with Borland compiler */
#if defined(_WIN32) && defined(__BORLANDC__)
  #undef LIBIOJSPUBFUN
  #undef LIBIOJSPUBVAR
  #undef LIBIOJSCALL
  #if defined(IN_LIBLIBIOJS) && !defined(LIBLIBIOJS_STATIC)
    #define LIBIOJSPUBFUN __declspec(dllexport)
    #define LIBIOJSPUBVAR __declspec(dllexport) extern
  #else
    #define LIBIOJSPUBFUN
    #if !defined(LIBLIBIOJS_STATIC)
      #define LIBIOJSPUBVAR __declspec(dllimport) extern
    #else
      #define LIBIOJSPUBVAR extern
    #endif
  #endif
  #define LIBIOJSCALL __cdecl
  #if !defined _REENTRANT
    #define _REENTRANT
  #endif
#endif

/* Windows platform with GNU compiler (Mingw) */
#if defined(_WIN32) && defined(__MINGW32__)
  #undef LIBIOJSPUBFUN
  #undef LIBIOJSPUBVAR
  #undef LIBIOJSCALL
/*
  #if defined(IN_LIBLIBIOJS) && !defined(LIBLIBIOJS_STATIC)
*/
  #if !defined(LIBLIBIOJS_STATIC)
    #define LIBIOJSPUBFUN __declspec(dllexport)
    #define LIBIOJSPUBVAR __declspec(dllexport) extern
  #else
    #define LIBIOJSPUBFUN
    #if !defined(LIBLIBIOJS_STATIC)
      #define LIBIOJSPUBVAR __declspec(dllimport) extern
    #else
      #define LIBIOJSPUBVAR extern
    #endif
  #endif
  #define LIBIOJSCALL __cdecl
  #if !defined _REENTRANT
    #define _REENTRANT
  #endif
#endif

/* Cygwin platform, GNU compiler */
#if defined(_WIN32) && defined(__CYGWIN__)
  #undef LIBIOJSPUBFUN
  #undef LIBIOJSPUBVAR
  #undef LIBIOJSCALL
  #if defined(IN_LIBLIBIOJS) && !defined(LIBLIBIOJS_STATIC)
    #define LIBIOJSPUBFUN __declspec(dllexport)
    #define LIBIOJSPUBVAR __declspec(dllexport)
  #else
    #define LIBIOJSPUBFUN
    #if !defined(LIBLIBIOJS_STATIC)
      #define LIBIOJSPUBVAR __declspec(dllimport) extern
    #else
      #define LIBIOJSPUBVAR
    #endif
  #endif
  #define LIBIOJSCALL __cdecl
#endif

/* Compatibility */
#if !defined(LIBLIBIOJS_PUBLIC)
#define LIBLIBIOJS_PUBLIC LIBIOJSPUBVAR
#endif

#endif /* __LIBIOJS_EXPORTS_H__ */
