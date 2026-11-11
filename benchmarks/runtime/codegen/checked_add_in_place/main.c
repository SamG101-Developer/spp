#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t hide_u64(uint64_t x) {
  volatile uint64_t v = x;
  return v;
}

int main(void) {
  uint64_t n = hide_u64(4096);
  int64_t start = (int64_t) hide_u64(1);
  int64_t k = (int64_t) hide_u64(3);
  int64_t passes = (int64_t) hide_u64(50000);
  int64_t *x = malloc(n * sizeof *x);
  for (uint64_t i = 0; i < n; ++i) { x[i] = start; }
  for (int64_t pass = 0; pass < passes; ++pass) {
    for (uint64_t i = 0; i < n; ++i) { x[i] = x[i] + k; }
  }
  printf("%lld\n", (long long) (x[0] + x[4095]));
  free(x);
  return 0;
}
