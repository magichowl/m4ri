/**
 * \file packedmatrix.h
 * \brief Dense matrices over GF(2) represented as a bit field.
 *
 * \author Gregory Bard <bard@fordham.edu>
 * \author Martin Albrecht <M.R.Albrecht@rhul.ac.uk>
 * \author Carlo Wood <carlo@alinoe.com>
 */

#ifndef M4RI_PACKEDMATRIX_H
#define M4RI_PACKEDMATRIX_H

/*******************************************************************
*
*                M4RI: Linear Algebra over GF(2)
*
*    Copyright (C) 2007, 2008 Gregory Bard <bard@fordham.edu>
*    Copyright (C) 2008-2010 Martin Albrecht <M.R.Albrecht@rhul.ac.uk>
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
*
********************************************************************/

#include "m4ri_config.h"

#include <math.h>
#include <assert.h>
#include <stdio.h>

#if __M4RI_HAVE_SSE2
#include <emmintrin.h>
#endif

#include "misc.h"
#include "debug_dump.h"

#if __M4RI_HAVE_SSE2
/**
 * \brief SSE2 cutoff in words.
 *
 * Cutoff in words after which row length SSE2 instructions should be
 * used.
 */

#define __M4RI_SSE2_CUTOFF 10
#endif

/**
 * Maximum number of words allocated for one mzd_t block.
 *
 * \note This value must fit in an int, even though it's type is size_t.
 */

#define __M4RI_MAX_MZD_BLOCKSIZE (((size_t)1) << 27)

/**
 * \brief Matrix multiplication block-ing dimension.
 * 
 * Defines the number of rows of the matrix A that are
 * processed as one block during the execution of a multiplication
 * algorithm.
 */

#define __M4RI_MUL_BLOCKSIZE MIN(((int)sqrt((double)(4 * __M4RI_CPU_L2_CACHE))) / 2, 2048)

typedef struct {
  size_t size;
  word* begin;
  word* end;
} mzd_block_t;

/**
 * \brief Dense matrices over GF(2). 
 * 
 * The most fundamental data type in this library.
 */

typedef struct mzd_t {
  /**
   * Number of rows.
   */

  rci_t nrows;

  /**
   * Number of columns.
   */

  rci_t ncols;

  /**
   * Number of words with valid bits.
   *
   * width = ceil((ncols + offset) / m4ri_radix)
   */

  wi_t width; 

  /**
   * Offset in words between rows.
   *
   * rowstride = (width < mzd_paddingwidth || (width & 1) == 0) ? width : width + 1;
   * where width is the width of the underlaying non-windowed matrix.
   */

  wi_t rowstride;

  /**
   * Offset in words from start of block to first word.
   *
   * rows[0] = blocks[0].begin + offset_vector;
   * This, together with rowstride, makes the rows array obsolete.
   */

  wi_t offset_vector;

  /**
   * Number of rows to the first row counting from the start of the first block.
   */

  wi_t row_offset;

  /**
   * column offset of the first column.
   */

  uint16_t offset;

  /**
   * Booleans to speed up things.
   *
   * The bits have the following meaning:
   *
   * 0: Has non-zero offset (and thus is windowed).
   * 1: Has non-zero excess.
   * 2: Is windowed, but has zero offset.
   * 3: Is windowed, but has zero excess.
   * 4: Is windowed, but owns the blocks allocations.
   * 5: Spans more than 1 block.
   */

  uint8_t flags;

  /**
   * blockrows_log = log2(blockrows);
   * where blockrows is the number of rows in one block, which is a power of 2.
   */

  uint8_t blockrows_log;

#if 0	// Commented out in order to keep the size of mzd_t 64 bytes (one cache line). This could be added back if rows was ever removed.
  /**
   * blockrows_mask = blockrows - 1;
   * where blockrows is the number of rows in one block, which is a power of 2.
   */

  int blockrows_mask;
#endif

  /**
   * Mask for valid bits in the word with the highest index (width - 1).
   */

  word high_bitmask;

  /**
   * Mask for valid bits in the word with the lowest index (0).
   */

  word low_bitmask;

  /**
   * Contains pointers to the actual blocks of memory containing the
   * values packed into words of size m4ri_radix.
   */

  mzd_block_t *blocks;

  /**
   * Address of first word in each row, so the first word of row i is
   * is m->rows[i]
   */

  word **rows;

} mzd_t;

/**
 * \brief The minimum width where padding occurs.
 */
static wi_t const mzd_paddingwidth = 3;

static uint8_t const mzd_flag_nonzero_offset = 0x1;
static uint8_t const mzd_flag_nonzero_excess = 0x2;
static uint8_t const mzd_flag_windowed_zerooffset = 0x4;
static uint8_t const mzd_flag_windowed_zeroexcess = 0x8;
static uint8_t const mzd_flag_windowed_ownsblocks = 0x10;
static uint8_t const mzd_flag_multiple_blocks = 0x20;

/**
 * \brief Test if a matrix is windowed.
 *
 * \param M Matrix
 *
 * \return a non-zero value if the matrix is windowed, otherwise return zero.
 */
static inline int mzd_is_windowed(mzd_t const *M) {
  return M->flags & (mzd_flag_nonzero_offset | mzd_flag_windowed_zerooffset);
}

/**
 * \brief Test if this mzd_t should free blocks.
 *
 * \param M Matrix
 *
 * \return TRUE iff blocks is non-zero and should be freed upon a call to mzd_free.
 */
static inline int mzd_owns_blocks(mzd_t const *M) {
  return M->blocks && (!mzd_is_windowed(M) || ((M->flags & mzd_flag_windowed_ownsblocks)));
}

/**
 * \brief Get a pointer the first word.
 *
 * \param M Matrix
 *
 * \return a pointer to the first word of the first row.
 */

static inline word* mzd_first_row(mzd_t const *M) {
  word* result = M->blocks[0].begin + M->offset_vector;
  assert(M->nrows == 0 || result == M->rows[0]);
  return result;
}

/**
 * \brief Get a pointer to the first word in block n.
 *
 * Use mzd_first_row for block number 0.
 *
 * \param M Matrix
 * \param n The block number. Must be larger than 0.
 *
 * \return a pointer to the first word of the first row in block n.
 */
static inline word* mzd_first_row_next_block(mzd_t const* M, int n) {
  assert(n > 0);
  return M->blocks[n].begin + M->offset_vector - M->row_offset * M->rowstride;
}

/**
 * \brief Convert row to blocks index.
 *
 * \param M Matrix.
 * \param row The row to convert.
 *
 * \return the block number that contains this row.
 */

static inline int mzd_row_to_block(mzd_t const* M, rci_t row) {
  return (M->row_offset + row) >> M->blockrows_log;
}

/**
 * \brief Total number of rows in this block.
 *
 * Should be called with a constant n=0, or with
 * n > 0 when n is a variable, for optimization
 * reasons.
 *
 * \param M Matrix
 * \param n The block number.
 *
 * \return the total number of rows in this block.
 */

static inline wi_t mzd_rows_in_block(mzd_t const* M, int n) {
  if (__M4RI_UNLIKELY(M->flags & mzd_flag_multiple_blocks)) {
    if (__M4RI_UNLIKELY(n == 0)) {
      return (1 << M->blockrows_log) - M->row_offset;
    } else {
      int const last_block = mzd_row_to_block(M, M->nrows - 1); 
      if (n < last_block)
	return (1 << M->blockrows_log);
      return M->nrows + M->row_offset - (n << M->blockrows_log);
    }
  }
  return n ? 0 : M->nrows;
}

/**
 * \brief Get pointer to first word of row.
 *
 * \param M Matrix
 * \param row The row index.
 *
 * \return pointer to first word of the row.
 */

static inline word* mzd_row(mzd_t const* M, rci_t row) {
  wi_t big_vector = M->offset_vector + row * M->rowstride;
  word* result = M->blocks[0].begin + big_vector;
  if (__M4RI_UNLIKELY(M->flags & mzd_flag_multiple_blocks)) {
    int const n = (M->row_offset + row) >> M->blockrows_log;
    result = M->blocks[n].begin + big_vector - n * (M->blocks[0].size / sizeof(word));
  }
  assert(result == M->rows[row]);
  return result;
}

/**
 * \brief Create a new matrix of dimension r x c.
 *
 * Use mzd_free to kill it.
 *
 * \param r Number of rows
 * \param c Number of columns
 *
 */

mzd_t *mzd_init(rci_t const r, rci_t const c);

/**
 * \brief Free a matrix created with mzd_init.
 * 
 * \param A Matrix
 */

void mzd_free(mzd_t *A);


/**
 * \brief Create a window/view into the matrix M.
 *
 * A matrix window for M is a meta structure on the matrix M. It is
 * setup to point into the matrix so M \em must \em not be freed while the
 * matrix window is used.
 *
 * This function puts the restriction on the provided parameters that
 * all parameters must be within range for M which is not enforced
 * currently .
 *
 * Use mzd_free_window to free the window.
 *
 * \param M Matrix
 * \param lowr Starting row (inclusive)
 * \param lowc Starting column (inclusive)
 * \param highr End row (exclusive)
 * \param highc End column (exclusive)
 *
 */

mzd_t *mzd_init_window(mzd_t *M, rci_t const lowr, rci_t const lowc, rci_t const highr, rci_t const highc);

/**
 * \brief Create a const window/view into a const matrix M.
 *
 * See mzd_init_window, but for constant M.
 */

static inline mzd_t const *mzd_init_window_const(mzd_t const *M, rci_t const lowr, rci_t const lowc, rci_t const highr, rci_t const highc)
{
  return mzd_init_window((mzd_t*)M, lowr, lowc, highr, highc);
}

/**
 * \brief Free a matrix window created with mzd_init_window.
 * 
 * \param A Matrix
 */

#define mzd_free_window mzd_free

/**
 * \brief Swap the two rows rowa and rowb starting at startblock.
 * 
 * \param M Matrix with a zero offset.
 * \param rowa Row index.
 * \param rowb Row index.
 * \param startblock Start swapping only in this block.
 */
 
static inline void _mzd_row_swap(mzd_t *M, rci_t const rowa, rci_t const rowb, wi_t const startblock) {
  if ((rowa == rowb) || (startblock >= M->width))
    return;

  /* This is the case since we're only called from _mzd_pls_mmpf,
   * which makes the same assumption. Therefore we don't need
   * to take a mask_begin into account. */
  assert(M->offset == 0);

  wi_t width = M->width - startblock - 1;
  word *a = M->rows[rowa] + startblock;
  word *b = M->rows[rowb] + startblock;
  word tmp; 
  word const mask_end = __M4RI_LEFT_BITMASK((M->ncols + M->offset) % m4ri_radix);

  if (width != 0) {
    for(wi_t i = 0; i < width; ++i) {
      tmp = a[i];
      a[i] = b[i];
      b[i] = tmp;
    }
  }
  tmp = (a[width] ^ b[width]) & mask_end;
  a[width] ^= tmp;
  b[width] ^= tmp;

  __M4RI_DD_ROW(M, rowa);
  __M4RI_DD_ROW(M, rowb);
}

/**
 * \brief Swap the two rows rowa and rowb.
 * 
 * \param M Matrix
 * \param rowa Row index.
 * \param rowb Row index.
 */
 
static inline void mzd_row_swap(mzd_t *M, rci_t const rowa, rci_t const rowb) {
  if(rowa == rowb)
    return;

  wi_t width = M->width - 1;
  word *a = M->rows[rowa];
  word *b = M->rows[rowb];
  word const mask_begin = __M4RI_RIGHT_BITMASK(m4ri_radix - M->offset);
  word const mask_end = __M4RI_LEFT_BITMASK((M->ncols + M->offset) % m4ri_radix);

  word tmp = (a[0] ^ b[0]) & mask_begin;
  if (width != 0) {
    a[0] ^= tmp;
    b[0] ^= tmp;
    
    for(wi_t i = 1; i < width; ++i) {
      tmp = a[i];
      a[i] = b[i];
      b[i] = tmp;
    }
    tmp = (a[width] ^ b[width]) & mask_end;
    a[width] ^= tmp;
    b[width] ^= tmp;
    
  } else {
    tmp &= mask_end;
    a[0] ^= tmp;
    b[0] ^= tmp;
  }

  __M4RI_DD_ROW(M, rowa);
  __M4RI_DD_ROW(M, rowb);
}

/**
 * \brief copy row j from A to row i from B.
 *
 * The offsets of A and B must match and the number of columns of A
 * must be less than or equal to the number of columns of B.
 *
 * \param B Target matrix.
 * \param i Target row index.
 * \param A Source matrix.
 * \param j Source row index.
 */

void mzd_copy_row(mzd_t *B, rci_t i, mzd_t const *A, rci_t j);

/**
 * \brief Swap the two columns cola and colb.
 * 
 * \param M Matrix.
 * \param cola Column index.
 * \param colb Column index.
 */
 
void mzd_col_swap(mzd_t *M, rci_t const cola, rci_t const colb);

/**
 * \brief Swap the two columns cola and colb but only between start_row and stop_row.
 * 
 * \param M Matrix.
 * \param cola Column index.
 * \param colb Column index.
 * \param start_row Row index.
 * \param stop_row Row index (exclusive).
 */
 
static inline void mzd_col_swap_in_rows(mzd_t *M, rci_t const cola, rci_t const colb, rci_t const start_row, rci_t const stop_row) {
  if (cola == colb)
    return;

  rci_t const _cola = cola + M->offset;
  rci_t const _colb = colb + M->offset;

  /* Make sure that a_spill >= b_spill (larger if a_word == b_word) */
  int const swap_a_b = _cola % m4ri_radix < _colb % m4ri_radix;
  int const a_spill = swap_a_b ? _colb % m4ri_radix : _cola % m4ri_radix;
  int const b_spill = swap_a_b ? _cola % m4ri_radix : _colb % m4ri_radix;
  wi_t const a_word = swap_a_b ? _colb / m4ri_radix : _cola / m4ri_radix;
  wi_t const b_word = swap_a_b ? _cola / m4ri_radix : _colb / m4ri_radix;

  word const b_bm = m4ri_one << b_spill;
  int const coldiff = a_spill - b_spill;

  if (a_word == b_word)
  {
    for (rci_t i = start_row; i < stop_row; ++i)
    {
      word *vp = (M->rows[i] + a_word);
      word v = *vp;
      word x = ((v >> coldiff) ^ v) & b_bm;	/* Move column a on top of column b, and calcuate XOR. */
      x |= x << coldiff;			/* Duplicate this bit at both column positions. */
      *vp = v ^ x;				/* Swap column bits and store result. */
    }
    __M4RI_DD_MZD(M);
    return;
  }

  for (rci_t i = start_row; i < stop_row; ++i)
  {
    word *base = M->rows[i];
    word a = *(base + a_word);
    word b = *(base + b_word);

    word x = ((a >> coldiff) ^ b) & b_bm;	/* Move column a on top of column b, and calculate XOR (in place of column b). */
    b ^= x;					/* Assign bit from column a to b */
    a ^= x << coldiff;				/* Move the XOR bit to the place of column a and assign bit from column b to a */

    *(base + a_word) = a;
    *(base + b_word) = b;
  }

  __M4RI_DD_MZD(M);
}

/**
 * \brief Read the bit at position M[row,col].
 *
 * \param M Matrix
 * \param row Row index
 * \param col Column index
 *
 * \note No bounds checks whatsoever are performed.
 *
 */

static inline BIT mzd_read_bit(mzd_t const *M, rci_t const row, rci_t const col ) {
  return __M4RI_GET_BIT(M->rows[row][(col+M->offset)/m4ri_radix], (col+M->offset) % m4ri_radix);
}

/**
 * \brief Write the bit value to position M[row,col]
 * 
 * \param M Matrix
 * \param row Row index
 * \param col Column index
 * \param value Either 0 or 1 
 *
 * \note No bounds checks whatsoever are performed.
 *
 */

static inline void mzd_write_bit(mzd_t *M, rci_t const row, rci_t const col, BIT const value) {
  __M4RI_WRITE_BIT(M->rows[row][(col + M->offset) / m4ri_radix], (col + M->offset) % m4ri_radix, value);
}


/**
 * \brief XOR n bits from values to M starting a position (x,y).
 *
 * \param M Source matrix.
 * \param x Starting row.
 * \param y Starting column.
 * \param n Number of bits (<= m4ri_radix);
 * \param values Word with values;
 */

static inline void mzd_xor_bits(mzd_t const *M, rci_t const x, rci_t const y, int const n, word values) {
  int const spot = (y + M->offset) % m4ri_radix;
  wi_t const block = (y + M->offset) / m4ri_radix;
  M->rows[x][block] ^= values << spot;
  int const space = m4ri_radix - spot;
  if (n > space)
    M->rows[x][block + 1] ^= values >> space;
}

/**
 * \brief AND n bits from values to M starting a position (x,y).
 *
 * \param M Source matrix.
 * \param x Starting row.
 * \param y Starting column.
 * \param n Number of bits (<= m4ri_radix);
 * \param values Word with values;
 */

static inline void mzd_and_bits(mzd_t const *M, rci_t const x, rci_t const y, int const n, word values) {
  /* This is the best way, since this will drop out once we inverse the bits in values: */
  values >>= (m4ri_radix - n);	/* Move the bits to the lowest columns */

  int const spot = (y + M->offset) % m4ri_radix;
  wi_t const block = (y + M->offset) / m4ri_radix;
  M->rows[x][block] &= values << spot;
  int const space = m4ri_radix - spot;
  if (n > space)
    M->rows[x][block + 1] &= values >> space;
}

/**
 * \brief Clear n bits in M starting a position (x,y).
 *
 * \param M Source matrix.
 * \param x Starting row.
 * \param y Starting column.
 * \param n Number of bits (0 < n <= m4ri_radix);
 */

static inline void mzd_clear_bits(mzd_t const *M, rci_t const x, rci_t const y, int const n) {
  word values = m4ri_ffff >> (m4ri_radix - n);
  int const spot = (y + M->offset) % m4ri_radix;
  wi_t const block = (y + M->offset) / m4ri_radix;
  M->rows[x][block] &= ~(values << spot);
  int const space = m4ri_radix - spot;
  if (n > space)
    M->rows[x][block + 1] &= ~(values >> space);
}

/**
 * \brief Print a matrix to stdout. 
 *
 * The output will contain colons between every 4-th column.
 *
 * \param M Matrix
 */

void mzd_print(mzd_t const *M);

/**
 * \brief Print the matrix to stdout.
 *
 * \param M Matrix
 */

void mzd_print_tight(mzd_t const *M);

/**
 * \brief Add the rows sourcerow and destrow and stores the total in the row
 * destrow, but only begins at the column coloffset.
 *
 * \param M Matrix
 * \param dstrow Index of target row
 * \param srcrow Index of source row
 * \param coloffset Start column (0 <= coloffset < M->ncols)
 */

static inline void mzd_row_add_offset(mzd_t *M, rci_t dstrow, rci_t srcrow, rci_t coloffset) {
  assert(dstrow < M->nrows && srcrow < M->nrows && coloffset < M->ncols);
  coloffset += M->offset;
  wi_t const startblock= coloffset/m4ri_radix;
  wi_t wide = M->width - startblock;
  word *src = M->rows[srcrow] + startblock;
  word *dst = M->rows[dstrow] + startblock;
  word const mask_begin = __M4RI_RIGHT_BITMASK(m4ri_radix - coloffset % m4ri_radix);
  word const mask_end = __M4RI_LEFT_BITMASK((M->ncols + M->offset) % m4ri_radix);

  *dst++ ^= *src++ & mask_begin;
  --wide;

#if __M4RI_HAVE_SSE2 
  int not_aligned = __M4RI_ALIGNMENT(src,16) != 0;	/* 0: Aligned, 1: Not aligned */
  if (wide > not_aligned + 1)				/* Speed up for small matrices */
  {
    if (not_aligned) {
      *dst++ ^= *src++;
      --wide;
    }
    /* Now wide > 1 */
    __m128i* __src = (__m128i*)src;
    __m128i* __dst = (__m128i*)dst;
    __m128i* const eof = (__m128i*)((unsigned long)(src + wide) & ~0xFUL);
    do
    {
      __m128i xmm1 = _mm_xor_si128(*__dst, *__src);
      *__dst++ = xmm1;
    }
    while(++__src < eof);
    src  = (word*)__src;
    dst = (word*)__dst;
    wide = ((sizeof(word)*wide)%16)/sizeof(word);
  }
#endif
  wi_t i = -1;
  while(++i < wide)
    dst[i] ^= src[i];
  /* 
   * Revert possibly non-zero excess bits.
   * Note that i == wide here, and wide can be 0.
   * But really, src[wide - 1] is M->rows[srcrow][M->width - 1] ;)
   * We use i - 1 here to let the compiler know these are the same addresses
   * that we last accessed, in the previous loop.
   */
  dst[i - 1] ^= src[i - 1] & ~mask_end;

  __M4RI_DD_ROW(M, dstrow);
}

/**
 * \brief Add the rows sourcerow and destrow and stores the total in
 * the row destrow.
 *
 * \param M Matrix
 * \param sourcerow Index of source row
 * \param destrow Index of target row
 *
 * \note this can be done much faster with mzd_combine.
 */

void mzd_row_add(mzd_t *M, rci_t const sourcerow, rci_t const destrow);

/**
 * \brief Transpose a matrix.
 *
 * This function uses the fact that:
\verbatim
   [ A B ]T    [AT CT]
   [ C D ]  =  [BT DT] 
 \endverbatim 
 * and thus rearranges the blocks recursively. 
 *
 * \param DST Preallocated return matrix, may be NULL for automatic creation.
 * \param A Matrix
 */

mzd_t *mzd_transpose(mzd_t *DST, mzd_t const *A);

/**
 * \brief Naive cubic matrix multiplication.
 *
 * That is, compute C such that C == AB.
 *
 * \param C Preallocated product matrix, may be NULL for automatic creation.
 * \param A Input matrix A.
 * \param B Input matrix B.
 *
 * \note Normally, if you will multiply several times by b, it is
 * smarter to calculate bT yourself, and keep it, and then use the
 * function called _mzd_mul_naive
 *
 */
mzd_t *mzd_mul_naive(mzd_t *C, mzd_t const *A, mzd_t const *B);

/**
 * \brief Naive cubic matrix multiplication and addition
 *
 * That is, compute C such that C == C + AB.
 *
 * \param C Preallocated product matrix.
 * \param A Input matrix A.
 * \param B Input matrix B.
 *
 * \note Normally, if you will multiply several times by b, it is
 * smarter to calculate bT yourself, and keep it, and then use the
 * function called _mzd_mul_naive
 */

mzd_t *mzd_addmul_naive(mzd_t *C, mzd_t const *A, mzd_t const *B);

/**
 * \brief Naive cubic matrix multiplication with the pre-transposed B.
 *
 * That is, compute C such that C == AB^t.
 *
 * \param C Preallocated product matrix.
 * \param A Input matrix A.
 * \param B Pre-transposed input matrix B.
 * \param clear Whether to clear C before accumulating AB
 */

mzd_t *_mzd_mul_naive(mzd_t *C, mzd_t const *A, mzd_t const *B, int const clear);

/**
 * \brief Matrix multiplication optimized for v*A where v is a vector.
 *
 * \param C Preallocated product matrix.
 * \param v Input matrix v.
 * \param A Input matrix A.
 * \param clear If set clear C first, otherwise add result to C.
 *
 */
mzd_t *_mzd_mul_va(mzd_t *C, mzd_t const *v, mzd_t const *A, int const clear);

/**
 * \brief Fill matrix M with uniformly distributed bits.
 *
 * \param M Matrix
 *
 * \todo Allow the user to provide a RNG callback.
 *
 * \wordoffset
 */

void mzd_randomize(mzd_t *M);

/**
 * \brief Set the matrix M to the value equivalent to the integer
 * value provided.
 *
 * Specifically, this function does nothing if value%2 == 0 and
 * returns the identity matrix if value%2 == 1.
 *
 * If the matrix is not square then the largest possible square
 * submatrix is set to the identity matrix.
 *
 * \param M Matrix
 * \param value Either 0 or 1
 */

void mzd_set_ui(mzd_t *M, unsigned int const value);

/**
 * \brief Gaussian elimination.
 * 
 * This will do Gaussian elimination on the matrix m but will start
 * not at column 0 necc but at column startcol. If full=FALSE, then it
 * will do triangular style elimination, and if full=TRUE, it will do
 * Gauss-Jordan style, or full elimination.
 * 
 * \param M Matrix
 * \param startcol First column to consider for reduction.
 * \param full Gauss-Jordan style or upper triangular form only.
 *
 * \wordoffset
 */

rci_t mzd_gauss_delayed(mzd_t *M, rci_t const startcol, int const full);

/**
 * \brief Gaussian elimination.
 * 
 * This will do Gaussian elimination on the matrix m.  If full=FALSE,
 *  then it will do triangular style elimination, and if full=TRUE,
 *  it will do Gauss-Jordan style, or full elimination.
 *
 * \param M Matrix
 * \param full Gauss-Jordan style or upper triangular form only.
 *
 * \wordoffset
 * 
 * \sa mzd_echelonize_m4ri(), mzd_echelonize_pluq()
 */

rci_t mzd_echelonize_naive(mzd_t *M, int const full);

/**
 * \brief Return TRUE if A == B.
 *
 * \param A Matrix
 * \param B Matrix
 *
 * \wordoffset
 */

int mzd_equal(mzd_t const *A, mzd_t const *B);

/**
 * \brief Return -1,0,1 if if A < B, A == B or A > B respectively.
 *
 * \param A Matrix.
 * \param B Matrix.
 *
 * \note This comparison is not well defined mathematically and
 * relatively arbitrary since elements of GF(2) don't have an
 * ordering.
 *
 * \wordoffset
 */

int mzd_cmp(mzd_t const *A, mzd_t const *B);

/**
 * \brief Copy matrix  A to DST.
 *
 * \param DST May be NULL for automatic creation.
 * \param A Source matrix.
 */

mzd_t *mzd_copy(mzd_t *DST, mzd_t const *A);

/**
 * \brief Concatenate B to A and write the result to C.
 * 
 * That is,
 *
 \verbatim
 [ A ], [ B ] -> [ A  B ] = C
 \endverbatim
 *
 * The inputs are not modified but a new matrix is created.
 *
 * \param C Matrix, may be NULL for automatic creation
 * \param A Matrix
 * \param B Matrix
 *
 * \note This is sometimes called augment.
 *
 * \wordoffset
 */

mzd_t *mzd_concat(mzd_t *C, mzd_t const *A, mzd_t const *B);

/**
 * \brief Stack A on top of B and write the result to C.
 *
 * That is, 
 *
 \verbatim
 [ A ], [ B ] -> [ A ] = C
                 [ B ]
 \endverbatim
 *
 * The inputs are not modified but a new matrix is created.
 *
 * \param C Matrix, may be NULL for automatic creation
 * \param A Matrix
 * \param B Matrix
 *
 * \wordoffset
 */

mzd_t *mzd_stack(mzd_t *C, mzd_t const *A, mzd_t const *B);

/**
 * \brief Copy a submatrix.
 * 
 * Note that the upper bounds are not included.
 *
 * \param S Preallocated space for submatrix, may be NULL for automatic creation.
 * \param M Matrix
 * \param lowr start rows
 * \param lowc start column
 * \param highr stop row (this row is \em not included)
 * \param highc stop column (this column is \em not included)
 */
mzd_t *mzd_submatrix(mzd_t *S, mzd_t const *M, rci_t const lowr, rci_t const lowc, rci_t const highr, rci_t const highc);

/**
 * \brief Invert the matrix target using Gaussian elimination. 
 *
 * To avoid recomputing the identity matrix over and over again, I may
 * be passed in as identity parameter.
 *
 * \param INV Preallocated space for inversion matrix, may be NULL for automatic creation.
 * \param A Matrix to be reduced.
 * \param I Identity matrix.
 *
 * \wordoffset
 */

mzd_t *mzd_invert_naive(mzd_t *INV, mzd_t const *A, mzd_t const *I);

/**
 * \brief Set C = A+B.
 *
 * C is also returned. If C is NULL then a new matrix is created which
 * must be freed by mzd_free.
 *
 * \param C Preallocated sum matrix, may be NULL for automatic creation.
 * \param A Matrix
 * \param B Matrix
 */

mzd_t *mzd_add(mzd_t *C, mzd_t const *A, mzd_t const *B);

/**
 * \brief Same as mzd_add but without any checks on the input.
 *
 * \param C Preallocated sum matrix, may be NULL for automatic creation.
 * \param A Matrix
 * \param B Matrix
 *
 * \wordoffset
 */

mzd_t *_mzd_add(mzd_t *C, mzd_t const *A, mzd_t const *B);

/**
 * \brief Same as mzd_add.
 *
 * \param C Preallocated difference matrix, may be NULL for automatic creation.
 * \param A Matrix
 * \param B Matrix
 *
 * \wordoffset
 */

#define mzd_sub mzd_add

/**
 * \brief Same as mzd_sub but without any checks on the input.
 *
 * \param C Preallocated difference matrix, may be NULL for automatic creation.
 * \param A Matrix
 * \param B Matrix
 *
 * \wordoffset
 */

#define _mzd_sub _mzd_add



/**
 * Get n bits starting a position (x,y) from the matrix M.
 *
 * \param M Source matrix.
 * \param x Starting row.
 * \param y Starting column.
 * \param n Number of bits (<= m4ri_radix);
 */ 

static inline word mzd_read_bits(mzd_t const *M, rci_t const x, rci_t const y, int const n) {
  int const spot = (y + M->offset) % m4ri_radix;
  wi_t const block = (y + M->offset) / m4ri_radix;
  int const spill = spot + n - m4ri_radix;
  word temp = (spill <= 0) ? M->rows[x][block] << -spill : (M->rows[x][block + 1] << (m4ri_radix - spill)) | (M->rows[x][block] >> spill);
  return temp >> (m4ri_radix - n);
}


/**
 * \brief row3[col3:] = row1[col1:] + row2[col2:]
 * 
 * Adds row1 of SC1, starting with startblock1 to the end, to
 * row2 of SC2, starting with startblock2 to the end. This gets stored
 * in DST, in row3, starting with startblock3.
 *
 * \param DST destination matrix
 * \param row3 destination row for matrix dst
 * \param startblock3 starting block to work on in matrix dst
 * \param SC1 source matrix
 * \param row1 source row for matrix sc1
 * \param startblock1 starting block to work on in matrix sc1
 * \param SC2 source matrix
 * \param startblock2 starting block to work on in matrix sc2
 * \param row2 source row for matrix sc2
 *
 */

void mzd_combine(mzd_t *DST, rci_t const row3, wi_t const startblock3,
		 mzd_t const *SC1, rci_t const row1, wi_t const startblock1, 
		 mzd_t const *SC2, rci_t const row2, wi_t const startblock2);


/**
 * \brief c_row[c_startblock:] = a_row[a_startblock:] + b_row[b_startblock:] for different offsets
 * 
 * Adds a_row of A, starting with a_startblock to the end, to
 * b_row of B, starting with b_startblock to the end. This gets stored
 * in C, in c_row, starting with c_startblock.
 *
 * \param C destination matrix
 * \param c_row destination row for matrix C
 * \param c_startblock starting block to work on in matrix C
 * \param A source matrix
 * \param a_row source row for matrix A
 * \param a_startblock starting block to work on in matrix A
 * \param B source matrix
 * \param b_row source row for matrix B
 * \param b_startblock starting block to work on in matrix B
 *
 */

static inline void mzd_combine_weird(mzd_t *C,       rci_t const c_row, wi_t const c_startblock,
                                     mzd_t const *A, rci_t const a_row, wi_t const a_startblock, 
                                     mzd_t const *B, rci_t const b_row, wi_t const b_startblock) {
  word tmp;
  rci_t i = 0;


  for(; i + m4ri_radix <= A->ncols; i += m4ri_radix) {
    tmp = mzd_read_bits(A, a_row, i, m4ri_radix) ^ mzd_read_bits(B, b_row, i, m4ri_radix);
    mzd_clear_bits(C, c_row, i, m4ri_radix);
    mzd_xor_bits(C, c_row, i, m4ri_radix, tmp);
  }
  if(A->ncols - i) {
    tmp = mzd_read_bits(A, a_row, i, (A->ncols - i)) ^ mzd_read_bits(B, b_row, i, (B->ncols - i));
    mzd_clear_bits(C, c_row, i, (C->ncols - i));
    mzd_xor_bits(C, c_row, i, (C->ncols - i), tmp);
  }

  __M4RI_DD_MZD(C);
}

/**
 * \brief a_row[a_startblock:] += b_row[b_startblock:] for offset 0
 * 
 * Adds a_row of A, starting with a_startblock to the end, to
 * b_row of B, starting with b_startblock to the end. This gets stored
 * in A, in a_row, starting with a_startblock.
 *
 * \param A destination matrix
 * \param a_row destination row for matrix C
 * \param a_startblock starting block to work on in matrix C
 * \param B source matrix
 * \param b_row source row for matrix B
 * \param b_startblock starting block to work on in matrix B
 *
 */

static inline void mzd_combine_even_in_place(mzd_t *A,       rci_t const a_row, wi_t const a_startblock,
                                             mzd_t const *B, rci_t const b_row, wi_t const b_startblock) {

  wi_t wide = A->width - a_startblock - 1;

  word *a = A->rows[a_row] + a_startblock;
  word *b = B->rows[b_row] + b_startblock;
  
#if __M4RI_HAVE_SSE2
  if(wide > __M4RI_SSE2_CUTOFF) {
    /** check alignments **/
    if (__M4RI_ALIGNMENT(a,16)) {
      *a++ ^= *b++;
      wide--;
    }
    
    if (__M4RI_ALIGNMENT(a, 16) == 0 && __M4RI_ALIGNMENT(b, 16) == 0) {
      __m128i *a128 = (__m128i*)a;
      __m128i *b128 = (__m128i*)b;
      const __m128i *eof = (__m128i*)((unsigned long)(a + wide) & ~0xFUL);
      
      do {
        *a128 = _mm_xor_si128(*a128, *b128);
        ++b128;
        ++a128;
      } while(a128 < eof);
      
      a = (word*)a128;
      b = (word*)b128;
      wide = ((sizeof(word) * wide) % 16) / sizeof(word);
    }
  }
#endif // __M4RI_HAVE_SSE2

  if (wide > 0) {
    wi_t n = (wide + 7) / 8;
    switch (wide % 8) {
    case 0: do { *(a++) ^= *(b++);
    case 7:      *(a++) ^= *(b++);
    case 6:      *(a++) ^= *(b++);
    case 5:      *(a++) ^= *(b++);
    case 4:      *(a++) ^= *(b++);
    case 3:      *(a++) ^= *(b++);
    case 2:      *(a++) ^= *(b++);
    case 1:      *(a++) ^= *(b++);
    } while (--n > 0);
    }
  }

  *a ^= *b & __M4RI_LEFT_BITMASK(A->ncols%m4ri_radix);

  __M4RI_DD_MZD(A);
}


/**
 * \brief c_row[c_startblock:] = a_row[a_startblock:] + b_row[b_startblock:] for offset 0
 * 
 * Adds a_row of A, starting with a_startblock to the end, to
 * b_row of B, starting with b_startblock to the end. This gets stored
 * in C, in c_row, starting with c_startblock.
 *
 * \param C destination matrix
 * \param c_row destination row for matrix C
 * \param c_startblock starting block to work on in matrix C
 * \param A source matrix
 * \param a_row source row for matrix A
 * \param a_startblock starting block to work on in matrix A
 * \param B source matrix
 * \param b_row source row for matrix B
 * \param b_startblock starting block to work on in matrix B
 *
 */

static inline void mzd_combine_even(mzd_t *C,       rci_t const c_row, wi_t const c_startblock,
                                    mzd_t const *A, rci_t const a_row, wi_t const a_startblock, 
                                    mzd_t const *B, rci_t const b_row, wi_t const b_startblock) {

  wi_t wide = A->width - a_startblock - 1;
  word *a = A->rows[a_row] + a_startblock;
  word *b = B->rows[b_row] + b_startblock;
  word *c = C->rows[c_row] + c_startblock;
  
  /* /\* this is a corner case triggered by Strassen multiplication */
  /*  * which assumes certain (virtual) matrix sizes  */
  /*  * 2011/03/07: I don't think this was ever correct *\/ */
  /* if (a_row >= A->nrows) { */
  /*   assert(a_row < A->nrows); */
  /*   for(wi_t i = 0; i < wide; ++i) { */
  /*     c[i] = b[i]; */
  /*   } */
  /* } else { */
#if __M4RI_HAVE_SSE2
  if(wide > __M4RI_SSE2_CUTOFF) {
    /** check alignments **/
    if (__M4RI_ALIGNMENT(a,16)) {
      *c++ = *b++ ^ *a++;
      wide--;
    }
      
    if ( (__M4RI_ALIGNMENT(b, 16) | __M4RI_ALIGNMENT(c, 16)) == 0) {
      __m128i *a128 = (__m128i*)a;
      __m128i *b128 = (__m128i*)b;
      __m128i *c128 = (__m128i*)c;
      const __m128i *eof = (__m128i*)((unsigned long)(a + wide) & ~0xFUL);
      
      do {
        *c128 = _mm_xor_si128(*a128, *b128);
        ++c128;
        ++b128;
        ++a128;
      } while(a128 < eof);
      
      a = (word*)a128;
      b = (word*)b128;
      c = (word*)c128;
      wide = ((sizeof(word) * wide) % 16) / sizeof(word);
    }
  }
#endif // __M4RI_HAVE_SSE2

  if (wide > 0) {
    wi_t n = (wide + 7) / 8;
    switch (wide % 8) {
    case 0: do { *(c++) = *(a++) ^ *(b++);
    case 7:      *(c++) = *(a++) ^ *(b++);
    case 6:      *(c++) = *(a++) ^ *(b++);
    case 5:      *(c++) = *(a++) ^ *(b++);
    case 4:      *(c++) = *(a++) ^ *(b++);
    case 3:      *(c++) = *(a++) ^ *(b++);
    case 2:      *(c++) = *(a++) ^ *(b++);
    case 1:      *(c++) = *(a++) ^ *(b++);
    } while (--n > 0);
    }
  }
  *c ^= ((*a ^ *b ^ *c) & __M4RI_LEFT_BITMASK(C->ncols%m4ri_radix));

  __M4RI_DD_MZD(C);
}


/**
 * \brief Get n bits starting a position (x,y) from the matrix M.
 *
 * This function is in principle the same as mzd_read_bits,
 * but it explicitely returns an 'int' and is used as
 * index into an array (Gray code).
 */ 

static inline int mzd_read_bits_int(mzd_t const *M, rci_t const x, rci_t const y, int const n)
{
  return __M4RI_CONVERT_TO_INT(mzd_read_bits(M, x, y, n));
}


/**
 * \brief Zero test for matrix.
 *
 * \param A Input matrix.
 *
 */
int mzd_is_zero(mzd_t const *A);

/**
 * \brief Clear the given row, but only begins at the column coloffset.
 *
 * \param M Matrix
 * \param row Index of row
 * \param coloffset Column offset
 */

void mzd_row_clear_offset(mzd_t *M, rci_t const row, rci_t const coloffset);

/**
 * \brief Find the next nonzero entry in M starting at start_row and start_col. 
 *
 * This function walks down rows in the inner loop and columns in the
 * outer loop. If a nonzero entry is found this function returns 1 and
 * zero otherwise.
 *
 * If and only if a nonzero entry is found r and c are updated.
 *
 * \param M Matrix
 * \param start_row Index of row where to start search
 * \param start_col Index of column where to start search
 * \param r Row index updated if pivot is found
 * \param c Column index updated if pivot is found
 */

int mzd_find_pivot(mzd_t const *M, rci_t start_row, rci_t start_col, rci_t *r, rci_t *c);


/**
 * \brief Return the number of nonzero entries divided by nrows *
 * ncols
 *
 * If res = 0 then 100 samples per row are made, if res > 0 the
 * function takes res sized steps within each row (res = 1 uses every
 * word).
 *
 * \param A Matrix
 * \param res Resolution of sampling (in words)
 */

double mzd_density(mzd_t const *A, wi_t res);

/**
 * \brief Return the number of nonzero entries divided by nrows *
 * ncols considering only the submatrix starting at (r,c).
 *
 * If res = 0 then 100 samples per row are made, if res > 0 the
 * function takes res sized steps within each row (res = 1 uses every
 * word).
 *
 * \param A Matrix
 * \param res Resolution of sampling (in words)
 * \param r Row to start counting
 * \param c Column to start counting
 */

double _mzd_density(mzd_t const *A, wi_t res, rci_t r, rci_t c);


/**
 * \brief Return the first row with all zero entries.
 *
 * If no such row can be found returns nrows.
 *
 * \param A Matrix
 */

rci_t mzd_first_zero_row(mzd_t const *A);

#endif // M4RI_PACKEDMATRIX_H
