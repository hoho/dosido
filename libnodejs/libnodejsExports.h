/*
 * Summary: macros for marking symbols as exportable/importable.
 * Description: macros for marking symbols as exportable/importable.
 *
 * Copy: See Copyright for the status of this software.
 *
 * Author: Igor Zlatkovic <igor@zlatkovic.com>
 */

#ifndef __LIBNODEJS_EXPORTS_H__
#define __LIBNODEJS_EXPORTS_H__

/**
 * LIBNODEJSPUBFUN:
 * LIBNODEJSPUBFUN, LIBNODEJSPUBVAR, LIBNODEJSCALL
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
 * LIBNODEJSPUBFUN:
 *
 * Macros which declare an exportable function
 */
#define LIBNODEJSPUBFUN
/**
 * LIBNODEJSPUBVAR:
 *
 * Macros which declare an exportable variable
 */
#define LIBNODEJSPUBVAR extern
/**
 * LIBNODEJSCALL:
 *
 * Macros which declare the called convention for exported functions
 */
#define LIBNODEJSCALL

/** DOC_DISABLE */

/* Windows platform with MS compiler */
#if defined(_WIN32) && defined(_MSC_VER)
  #undef LIBNODEJSPUBFUN
  #undef LIBNODEJSPUBVAR
  #undef LIBNODEJSCALL
  #if defined(IN_LIBLIBNODEJS) && !defined(LIBLIBNODEJS_STATIC)
    #define LIBNODEJSPUBFUN __declspec(dllexport)
    #define LIBNODEJSPUBVAR __declspec(dllexport)
  #else
    #define LIBNODEJSPUBFUN
    #if !defined(LIBLIBNODEJS_STATIC)
      #define LIBNODEJSPUBVAR __declspec(dllimport) extern
    #else
      #define LIBNODEJSPUBVAR extern
    #endif
  #endif
  #define LIBNODEJSCALL __cdecl
  #if !defined _REENTRANT
    #define _REENTRANT
  #endif
#endif

/* Windows platform with Borland compiler */
#if defined(_WIN32) && defined(__BORLANDC__)
  #undef LIBNODEJSPUBFUN
  #undef LIBNODEJSPUBVAR
  #undef LIBNODEJSCALL
  #if defined(IN_LIBLIBNODEJS) && !defined(LIBLIBNODEJS_STATIC)
    #define LIBNODEJSPUBFUN __declspec(dllexport)
    #define LIBNODEJSPUBVAR __declspec(dllexport) extern
  #else
    #define LIBNODEJSPUBFUN
    #if !defined(LIBLIBNODEJS_STATIC)
      #define LIBNODEJSPUBVAR __declspec(dllimport) extern
    #else
      #define LIBNODEJSPUBVAR extern
    #endif
  #endif
  #define LIBNODEJSCALL __cdecl
  #if !defined _REENTRANT
    #define _REENTRANT
  #endif
#endif

/* Windows platform with GNU compiler (Mingw) */
#if defined(_WIN32) && defined(__MINGW32__)
  #undef LIBNODEJSPUBFUN
  #undef LIBNODEJSPUBVAR
  #undef LIBNODEJSCALL
/*
  #if defined(IN_LIBLIBNODEJS) && !defined(LIBLIBNODEJS_STATIC)
*/
  #if !defined(LIBLIBNODEJS_STATIC)
    #define LIBNODEJSPUBFUN __declspec(dllexport)
    #define LIBNODEJSPUBVAR __declspec(dllexport) extern
  #else
    #define LIBNODEJSPUBFUN
    #if !defined(LIBLIBNODEJS_STATIC)
      #define LIBNODEJSPUBVAR __declspec(dllimport) extern
    #else
      #define LIBNODEJSPUBVAR extern
    #endif
  #endif
  #define LIBNODEJSCALL __cdecl
  #if !defined _REENTRANT
    #define _REENTRANT
  #endif
#endif

/* Cygwin platform, GNU compiler */
#if defined(_WIN32) && defined(__CYGWIN__)
  #undef LIBNODEJSPUBFUN
  #undef LIBNODEJSPUBVAR
  #undef LIBNODEJSCALL
  #if defined(IN_LIBLIBNODEJS) && !defined(LIBLIBNODEJS_STATIC)
    #define LIBNODEJSPUBFUN __declspec(dllexport)
    #define LIBNODEJSPUBVAR __declspec(dllexport)
  #else
    #define LIBNODEJSPUBFUN
    #if !defined(LIBLIBNODEJS_STATIC)
      #define LIBNODEJSPUBVAR __declspec(dllimport) extern
    #else
      #define LIBNODEJSPUBVAR
    #endif
  #endif
  #define LIBNODEJSCALL __cdecl
#endif

/* Compatibility */
#if !defined(LIBLIBNODEJS_PUBLIC)
#define LIBLIBNODEJS_PUBLIC LIBNODEJSPUBVAR
#endif

#endif /* __LIBNODEJS_EXPORTS_H__ */
