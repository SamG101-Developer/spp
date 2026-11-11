#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t hide_u64(uint64_t x) {
  volatile uint64_t v = x;
  return v;
}

/* "n" LCG values from "seed", as main.spp and main.rs fill their Vec. */
static uint64_t *filled(uint64_t n, uint64_t seed) {
  uint64_t *v = malloc(n * sizeof *v);
  uint64_t s = seed;
  for (uint64_t i = 0; i < n; ++i) {
    s = s * 6364136223846793005u + 1442695040888963407u;
    v[i] = s >> 16;
  }
  return v;
}

int main(void) {
  uint64_t n = hide_u64(4096);
  uint64_t passes = hide_u64(50000);
  uint64_t *v = filled(n, hide_u64(12345));
  for (uint64_t pass = 0; pass < passes; ++pass) {
    for (uint64_t *p = v; p < v + n; ++p) { *p = *p * 3 + pass; }
  }
  uint64_t sum = 0;
  for (uint64_t i = 0; i < n; ++i) { sum += v[i]; }
  printf("%llu\n", (unsigned long long) sum);
  free(v);
  return 0;
}
