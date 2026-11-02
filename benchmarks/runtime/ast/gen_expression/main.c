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
  uint64_t n = hide_u64(50000000);
  uint64_t sum = 0;
  for (uint64_t x = 0; x < n; ++x) { sum = (sum + x * 3) % 1000000007; }
  printf("%llu\n", (unsigned long long) sum);
  return 0;
}
