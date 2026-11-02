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
  uint64_t n = hide_u64(250000);
  uint64_t acc = 0;
  for (uint64_t i = 0; i < n; ++i) {
    uint64_t found = 5000;
    for (uint64_t j = 0; j < 2000; ++j) {
      if ((i + j) % 977 == 0) { found = j; break; }
      if (j % 3 == 0) { continue; }
      acc += 1;
    }
    acc = acc + found;
  }
  printf("%llu\n", (unsigned long long) acc);
  return 0;
}
