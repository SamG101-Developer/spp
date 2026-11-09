#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t hide_u64(uint64_t x) {
  volatile uint64_t v = x;
  return v;
}

/* "n" small LCG values from "seed" (below 2^20, so no product or sum overflows), as main.spp and main.rs fill theirs. */
static int64_t *filled(uint64_t n, uint64_t seed) {
  int64_t *v = malloc(n * sizeof *v);
  uint64_t s = seed;
  for (uint64_t i = 0; i < n; ++i) {
    s = s * 6364136223846793005u + 1442695040888963407u;
    v[i] = (int64_t) (s >> 44);
  }
  return v;
}

int main(void) {
  uint64_t n = hide_u64(4096);
  int64_t passes = (int64_t) hide_u64(50000);
  uint64_t seed = hide_u64(12345);
  int64_t *a = filled(n, seed);
  uint64_t s = seed;
  for (uint64_t i = 0; i < n; ++i) { s = s * 6364136223846793005u + 1442695040888963407u; }
  int64_t *b = filled(n, s);
  int64_t sum = 0;
  for (int64_t pass = 0; pass < passes; ++pass) {
    int64_t dot = 0;
    for (uint64_t i = 0; i < n; ++i) { dot = dot + a[i] * b[i]; }
    sum = (sum + dot % 1000003 + pass) % 1000000007;
  }
  printf("%lld\n", (long long) sum);
  free(a);
  free(b);
  return 0;
}
