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

static void mix(uint64_t *acc, uint64_t i) { *acc = *acc ^ (i & 255); }

int main(void) {
  uint64_t n = hide_u64(100000000);
  uint64_t acc = 0;
  for (uint64_t i = 0; i < n; ++i) {
    {
      acc = (acc * 3 + i) % 1000003;
      mix(&acc, i);
    }
  }
  printf("%llu\n", (unsigned long long) acc);
  return 0;
}
