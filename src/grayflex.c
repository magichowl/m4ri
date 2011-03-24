/******************************************************************************
*
*                 M4RI: Linear Algebra over GF(2)
*
*    Copyright (C) 2007 Gregory Bard <gregory.bard@ieee.org> 
*    Copyright (C) 2007 Martin Albrecht <malb@informatik.uni-bremen.de> 
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

#include <stdio.h>
#include "misc.h"
#include "grayflex.h"

code **codebook = NULL;

int m4ri_gray_code(int number, int length) {
  int lastbit = 0;
  int res = 0;
  for(int i = length - 1; i >= 0; --i) {
    int bit = number & (1 << i);
    res |= (lastbit >> 1) ^ bit;
    lastbit = bit;
  }
  return res;
}

void m4ri_build_code(int *ord, int *inc, int l) {
  for(unsigned int i = 0 ; i < TWOPOW(l); ++i) {
    ord[i] = m4ri_gray_code(i, l);
  }

  for(int i = l; i > 0; --i) {
    for(unsigned int j = 1; j < TWOPOW(i) + 1; ++j) {
      inc[j * TWOPOW(l - i) - 1] = l - i;
    }
  }
}

void m4ri_build_all_codes() {
  if (codebook) {
    return;
  }
  codebook=(code**)m4ri_mm_calloc(MAXKAY+1, sizeof(code*));
  
  for(int k = 1; k < MAXKAY + 1; ++k) {
    codebook[k] = (code*)m4ri_mm_calloc(sizeof(code), 1);
    codebook[k]->ord =(int*)m4ri_mm_calloc(TWOPOW(k), sizeof(int));
    codebook[k]->inc =(int*)m4ri_mm_calloc(TWOPOW(k), sizeof(int));
    m4ri_build_code(codebook[k]->ord, codebook[k]->inc, k);
  }
}

void m4ri_destroy_all_codes() {
  if (!codebook) {
    return;
  }
  for(int i = 1; i < MAXKAY + 1; ++i) {
    m4ri_mm_free(codebook[i]->inc);
    m4ri_mm_free(codebook[i]->ord);
    m4ri_mm_free(codebook[i]);
  }
  m4ri_mm_free(codebook);
  codebook = NULL;
}

static int log2_floor(int v) {
  static unsigned const int b[] = { 0x2, 0xC, 0xF0, 0xFF00, 0xFFFF0000 };
  static unsigned const int S[] = { 1, 2, 4, 8, 16 };
  unsigned int r = 0;
  for (int i = 4; i >= 0; --i)
  {
    if ((v & b[i]))
    {
      v >>= S[i];
      r |= S[i];
    } 
  }
  return r;
}

int m4ri_opt_k(int a, int b, int c) {
  int n = MIN(a, b);
  int res = MIN(MAXKAY, MAX(1, (int)(0.75 * (1 + log2_floor(n)))) );
  return res;
}
