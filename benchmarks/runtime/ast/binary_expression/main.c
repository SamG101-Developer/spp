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
  uint64_t n = hide_u64(100000000);
  uint64_t a = hide_u64(1), b = hide_u64(7), c = 0;
  for (uint64_t i = 0; i < n; ++i) {
    a = (a * 31 + b) % 1000003;
    b = ((b ^ a) & 0xffff) | (i >> 3 & 1);
    c = c + a / (b + 1) - (c >> 7);
    if (a < b) { c += 1; }
  }
  printf("%llu\n%llu\n%llu\n", (unsigned long long) a, (unsigned long long) b, (unsigned long long) c);
  return 0;
}
