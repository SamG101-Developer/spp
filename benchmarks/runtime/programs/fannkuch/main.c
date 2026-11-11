#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t hide_u64(uint64_t x) {
  volatile uint64_t v = x;
  return v;
}

static int64_t hide_s64(int64_t x) {
  volatile int64_t v = x;
  return v;
}

static double hide_f64(double x) {
  volatile double v = x;
  return v;
}

int main(void) {
  size_t n = (size_t) hide_u64(10);
  int64_t perm[16] = {0}, perm1[16] = {0}, count[16] = {0};
  int64_t max_flips = 0, checksum = 0, perm_count = 0;
  size_t r = n;
  for (size_t i = 0; i < n; ++i) { perm1[i] = (int64_t) i; }
  for (;;) {
    while (r != 1) { count[r - 1] = (int64_t) r; r -= 1; }
    for (size_t i = 0; i < n; ++i) { perm[i] = perm1[i]; }
    int64_t flips = 0;
    size_t k = (size_t) perm[0];
    while (k != 0) {
      size_t lo = 0, hi = k;
      while (lo < hi) {
        int64_t t = perm[lo];
        perm[lo] = perm[hi];
        perm[hi] = t;
        lo += 1;
        hi -= 1;
      }
      flips += 1;
      k = (size_t) perm[0];
    }
    if (flips > max_flips) { max_flips = flips; }
    if (perm_count % 2 == 0) { checksum += flips; } else { checksum -= flips; }
    for (;;) {
      if (r == n) {
        printf("%lld\n%lld\n", (long long) checksum, (long long) max_flips);
        return 0;
      }
      int64_t perm0 = perm1[0];
      for (size_t i = 0; i < r; ++i) { perm1[i] = perm1[i + 1]; }
      perm1[r] = perm0;
      count[r] = count[r] - 1;
      if (count[r] > 0) { break; }
      r += 1;
    }
    perm_count += 1;
  }
}
