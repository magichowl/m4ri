/******************************************************************************
*
*                 M4RI: Linear Algebra over GF(2)
*
*    Copyright (C) 2007 Gregory Bard <gregory.bard@ieee.org> 
*    Copyright (C) 2011 Carlo Wood <carlo@alinoe.com>
*
*  Distributed under the terms of the GNU General Public License (GPL)
*  version 2 or higher.
*
*    This code is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*    General Public License for more details.
*
*  The full text of the GPL is available at:
*
*                  http://www.gnu.org/licenses/
******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _MSC_VER
#include <windows.h>
#endif

#ifndef HAVE_SSE2
#undef HAVE_MM_MALLOC
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "grayflex.h"
#include "misc.h"

/* blocks of memory we like to keep around for later re-use */
mmb_t m4ri_mmc_cache[M4RI_MMC_NBLOCKS];

void m4ri_die(const char *errormessage, ...) {
  va_list lst;
  va_start(lst, errormessage);
  vfprintf(stderr, errormessage, lst);
  va_end(lst);
  abort();
}

/* Warning: If colon, destination must have RADIX + (RADIX - 1) / 4 + 1 bytes available. */
void m4ri_word_to_str(char *destination, word data, int colon) {
  int j = 0;
  for (int i = 0; i < RADIX; ++i) {
    if (colon && (i % 4) == 0 && i != 0)
      destination[j++] = ':';
    if (GET_BIT(data, i))
      destination[j++] = '1';
    else 
      destination[j++] = ' ';
  }
  destination[j] = '\0';
}

word m4ri_random_word() {
  // random() only returns 31 bits, so we need three calls.
  word a0 = random();
  word a1 = random();
  word a2 = random();
  return a0 ^ (a1 << 24) ^ a2 << 48;
}

#ifdef __GNUC__
void __attribute__ ((constructor)) m4ri_init()
#else
void m4ri_init()
#endif
{
  m4ri_build_all_codes();
}
#ifdef __GNUC__
void __attribute__ ((destructor)) m4ri_fini()
#else
void m4ri_fini()
#endif
{
  m4ri_mmc_cleanup();
  m4ri_destroy_all_codes();
}

#ifdef _MSC_VER
BOOL WINAPI DllMain(
                    HINSTANCE hinstDLL,  // handle to DLL module
                    DWORD fdwReason,     // reason for calling function
                    LPVOID lpReserved )  // reserved
{
    // Perform actions based on the reason for calling.
  switch( fdwReason ) 
    { 
    case DLL_PROCESS_ATTACH:
      m4ri_build_all_codes();
      break;
      
    case DLL_THREAD_ATTACH:
      // Do thread-specific initialization.
      break;
      
    case DLL_THREAD_DETACH:
      // Do thread-specific cleanup.
      break;
      
    case DLL_PROCESS_DETACH:
      m4ri_mmc_cleanup();
      m4ri_destroy_all_codes();
      break;
    }
  return TRUE;  // Successful DLL_PROCESS_ATTACH.
}
#endif
