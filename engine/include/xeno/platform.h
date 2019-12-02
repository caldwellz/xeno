/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#ifndef _XENO_PLATFORM_H_
#define _XENO_PLATFORM_H_

#if (defined XBOX) || (defined NXDK)
  #define XENO_PLATFORM_NXDK
#endif


#if (defined DEBUG) && !(defined NDEBUG)
  #ifdef XENO_PLATFORM_NXDK
    #include <hal/debug.h>
  #else
    #include <stdio.h>
    #define debugPrint(...) printf(__VA_ARGS__)
  #endif
#else
  #define debugPrint(...)
#endif

#ifdef XENO_PLATFORM_NXDK
  #define debugSleep(n) Sleep(n)
#else
  #define debugSleep(n)
#endif

#endif //_XENO_PLATFORM_H_
