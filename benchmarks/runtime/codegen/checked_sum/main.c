#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t hide_u64(uint64_t x) {
  volatile uint64_t v = x;
  return v;
}

int main(void) {
  uint64_t n = hide_u64(4096);
  int64_t *x = malloc(n * sizeof *x);
  uint64_t s = hide_u64(12345);
  for (uint64_t i = 0; i < n; ++i) {
    s = s * 6364136223846793005u + 1442695040888963407u;
    x[i] = (int64_t) (s >> 44) - 524288;
  }
  int64_t passes = (int64_t) hide_u64(50000);
  int64_t total = 0;
  for (int64_t pass = 0; pass < passes; ++pass) {
    int64_t sum = 0;
    for (uint64_t i = 0; i < n; ++i) { sum = sum + x[i]; }
    total = (total + sum + pass) % 1000000007;
  }
  printf("%lld\n", (long long) total);
  free(x);
  return 0;
}
