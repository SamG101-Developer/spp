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
  uint64_t state = hide_u64(9), acc = 0;
  for (uint64_t i = 0; i < n; ++i) {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    uint64_t a = state >> 60, b = (state >> 56) & 15;
    uint64_t t = a == b ? a * 2 : a > b ? a - b : 1;
    uint64_t r = a < 4 ? b : 2;
    uint64_t o = a == 3 ? b + 1 : a ^ b;
    if (a > b) { acc += 1; }
    acc = acc + t + r + o;
  }
  printf("%llu\n", (unsigned long long) acc);
  return 0;
}
