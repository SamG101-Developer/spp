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
  uint64_t limit = hide_u64(1000000);
  uint64_t best = 0, best_n = 0, total = 0;
  for (uint64_t n = 1; n < limit; ++n) {
    uint64_t x = n, steps = 0;
    while (x != 1) {
      if (x % 2 == 0) { x = x / 2; } else { x = x * 3 + 1; }
      steps += 1;
    }
    total += steps;
    if (steps > best) { best = steps; best_n = n; }
  }
  printf("%llu\n%llu\n%llu\n", (unsigned long long) best_n, (unsigned long long) best, (unsigned long long) total);
  return 0;
}
