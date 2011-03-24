#include <stdlib.h>

#include "config.h"
#include "cpucycles.h"
#include "walltime.h"
#include "m4ri.h"

int main(int argc, char **argv) {
  if (argc != 3) {
    m4ri_die("Parameters m, n expected.\n");
  }
  rci_t m = (unsigned int)atoi(argv[1]);
  rci_t n = (unsigned int)atoi(argv[2]);
  mzd_t *B = mzd_init(m, n);
  mzd_t *U = mzd_init(m, m);
  mzd_randomize(B);
  mzd_randomize(U);
  for (rci_t i = 0; i < n; ++i){
    for (rci_t j = 0; j < i; ++j)
      mzd_write_bit(U,i,j, 0);
    mzd_write_bit(U,i,i, 1);
  }
  
  double clockZero = 0.0;
  double wt = walltime(&clockZero);
  unsigned long long t = cpucycles();
  mzd_trsm_upper_left(U, B, 0);
  printf("m: %5d, n: %5d, cpu cycles: %llu wall time: %lf\n", m.val(), n.val(), cpucycles() - t, walltime(&wt));

  mzd_free(B);
  mzd_free(U);
}
