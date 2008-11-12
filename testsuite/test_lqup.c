#include <stdlib.h>
#include "m4ri/m4ri.h"


int test_lqup_full_rank (int m, int n){
  packedmatrix* U = mzd_init (m,n);
  packedmatrix* L = mzd_init (m,m);
  packedmatrix* U2 = mzd_init (m,n);
  packedmatrix* L2 = mzd_init (m,m);
  packedmatrix* A = mzd_init (m,n);
  mzd_randomize (U);
  mzd_randomize (L);

  size_t i,j;
  for (i=0; i<m; ++i){
    for (j=0; j<i;++j)
      mzd_write_bit(U,i,j, 0);
    for (j=i+1; j<m;++j)
      mzd_write_bit(L,i,j, 0);
    mzd_write_bit(U,i,i, 1);
    mzd_write_bit(L,i,i, 1);
  }
  
  mzd_mul(A, L, U, 2048);

  packedmatrix* Acopy = mzd_copy (NULL,A);

  permutation* P = mzp_init(m);
  permutation* Q = mzp_init(n);
  mzd_lqup (A, P, Q, 2048);

  for (i=0; i<m; ++i){
    for (j=0; j<i;++j)
      mzd_write_bit (L2, i, j, mzd_read_bit(A,i,j));
    for (j=i+1; j<n;++j)
      mzd_write_bit (U2, i, j, mzd_read_bit(A,i,j));
  }
  
  for (i=0; i<n; ++i){
    mzd_write_bit(L2,i,i, 1);
    mzd_write_bit(U2,i,i, 1);
  }
  mzd_addmul(Acopy,L2,U2,0);

  int status = 0;
  for ( i=0; i<m; ++i)
    for ( j=0; j<n; ++j){
      if (mzd_read_bit (Acopy,i,j)){
	status = 1;
      }
    }
  if (status)
    printf(" FAILED.\n");
  else
    printf (" ... passed\n");
  mzd_free(U);
  mzd_free(L);
  mzd_free(U2);
  mzd_free(L2);
  mzd_free(A);
  mzd_free(Acopy);
  mzp_free(P);
  mzp_free(Q);
  return status;
}

int test_lqup_half_rank(size_t m, size_t n) {
  packedmatrix* U = mzd_init(m, n);
  packedmatrix* L = mzd_init(m, m);
  packedmatrix* U2 = mzd_init(m, n);
  packedmatrix* L2 = mzd_init(m, m);
  packedmatrix* A = mzd_init(m, n);
  mzd_randomize (U);
  mzd_randomize (L);

  size_t i,j;
  for (i=0; i<m; ++i){
    mzd_write_bit(U,i,i, 1);
    for (j=0; j<i;++j)
      mzd_write_bit(U,i,j, 0);
    if (i%2)
      for (j=i; j<n;++j)
	mzd_write_bit(U,i,j, 0);
    for (j=i+1; j<m;++j)
      mzd_write_bit(L,i,j, 0);
    mzd_write_bit(L,i,i, 1);
  }
  
  mzd_mul(A, L, U, 2048);

  packedmatrix* Acopy = mzd_copy (NULL,A);

  permutation* P = mzp_init(m);
  permutation* Q = mzp_init(n);
  int r;
  r=mzd_lqup (A, P, Q, 2048);

  for (i=0; i<r; ++i){
    for (j=0; j<i;++j)
      mzd_write_bit (L2, i, j, mzd_read_bit(A,i,j));
    for (j=i+1; j<n;++j)
      mzd_write_bit (U2, i, j, mzd_read_bit(A,i,j));
  }
  for (i=r; i<m; i++)
    for (j=0; j<r;++j)
      mzd_write_bit (L2, i, j, mzd_read_bit(A,i,j));
  for (i=0; i<r; ++i){
    mzd_write_bit(L2,i,i, 1);
    mzd_write_bit(U2,i,i, 1);
  }
  mzd_apply_p_left(Acopy, P);
  mzd_apply_p_right(Acopy, Q);
  mzd_addmul(Acopy,L2,U2,0);

  int status = 0;
  for ( i=0; i<m; ++i)
    for ( j=0; j<n; ++j){
      if (mzd_read_bit (Acopy,i,j)){
	status = 1;
      }
    }
  if (status)
    printf(" FAILED.\n");
  else
    printf (" ... passed\n");
  mzd_free(U);
  mzd_free(L);
  mzd_free(U2);
  mzd_free(L2);
  mzd_free(A);
  mzd_free(Acopy);
  mzp_free(P);
  mzp_free(Q);
  return status;
}

int test_lqup_random(size_t m, size_t n) {
  size_t i,j;
  packedmatrix* U = mzd_init(m, n);
  packedmatrix* L = mzd_init(m, m);
  packedmatrix* A = mzd_init(m, n);
  mzd_randomize(A);

  packedmatrix* Acopy = mzd_copy (NULL,A);

  permutation* P = mzp_init(m);
  permutation* Q = mzp_init(n);
  int r;
  r=mzd_lqup(A, P, Q, 0);
  printf(", rank: %5d ",r);

  for (i=0; i<r; ++i){
    for (j=0; j<i;++j)
      mzd_write_bit(L, i, j, mzd_read_bit(A,i,j));
    for (j=i+1; j<n;++j)
      mzd_write_bit(U, i, j, mzd_read_bit(A,i,j));
  }
  for (i=r; i<m; i++)
    for (j=0; j<r;++j)
      mzd_write_bit(L, i, j, mzd_read_bit(A,i,j));
  for (i=0; i<r; ++i){
    mzd_write_bit(L,i,i, 1);
    mzd_write_bit(U,i,i, 1);
  }

  mzd_apply_p_left(Acopy, P);
  mzd_apply_p_right(Acopy, Q);

  mzd_addmul(Acopy, L, U, 0);

  int status = 0;
  for ( i=0; i<m; ++i)
    for ( j=0; j<n; ++j){
      if (mzd_read_bit (Acopy,i,j)){
	status = 1;
        break;
      }
    }
  if (status)
    printf(" ... FAILED.\n");
  else
    printf (" ... passed\n");
  mzd_free(U);
  mzd_free(L);
  mzd_free(A);
  mzd_free(Acopy);
  mzp_free(P);
  mzp_free(Q);
  return status;
}


int main(int argc, char **argv) {
  int status = 0;

  printf("testing base case full rank n=37");
  status += test_lqup_full_rank(37,37);
  printf("testing base case full rank n=64");
  status += test_lqup_full_rank(64,64);
  printf("testing base case half rank n=64");
  status += test_lqup_half_rank(64,64);

  printf("testing LU block recursive algorithm full rank   n=97");
  status += test_lqup_full_rank(97,97);
  printf("testing LU block recursive algorithm full rank  n=128");
  status += test_lqup_full_rank(128,128);
  printf("testing LU block recursive algorithm full rank  n=150");
  status += test_lqup_full_rank(150,150);
  printf("testing LU block recursive algorithm full rank  n=256");
  status += test_lqup_full_rank(256,256);
  printf("testing LU block recursive algorithm full rank n=1024");
  status += test_lqup_full_rank(1024,1024);

  printf("testing LU block recursive algorithm half rank  n=128");
  status += test_lqup_half_rank(128,128);
  printf("testing LU block recursive algorithm half rank  n=150");
  status += test_lqup_half_rank(150,150);
  printf("testing LU block recursive algorithm half rank  n=256");
  status += test_lqup_half_rank(256,256);
  printf("testing LU block recursive algorithm half rank n=1024");
  status += test_lqup_half_rank(1024,1024);

  printf("testing LU block recursive algorithm random  n=128");
  status += test_lqup_random(128,128);
  printf("testing LU block recursive algorithm random  n=150");
  status += test_lqup_random(150,150);
  printf("testing LU block recursive algorithm random  n=256");
  status += test_lqup_random(256,256);
  printf("testing LU block recursive algorithm random n=1024");
  status += test_lqup_random(1024,1024);

  printf("testing LU block recursive algorithm random  n=128");
  status += test_lqup_random(128, 131);
  printf("testing LU block recursive algorithm random  n=150");
  status += test_lqup_random(132, 731);
  printf("testing LU block recursive algorithm random  n=256");
  status += test_lqup_random(252, 24);
  printf("testing LU block recursive algorithm random n=1024");
  status += test_lqup_random(1024,1022);


  if (!status) {
    printf("All tests passed.\n");
    return 0;
  } else {
    return -1;
  }
}
