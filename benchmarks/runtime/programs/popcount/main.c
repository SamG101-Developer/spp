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

static uint64_t pop(uint64_t x) {
  uint64_t a = x - ((x >> 1) & 0x5555555555555555ULL);
  uint64_t b = (a & 0x3333333333333333ULL) + ((a >> 2) & 0x3333333333333333ULL);
  uint64_t c = (b + (b >> 4)) & 0x0f0f0f0f0f0f0f0fULL;
  return (c * 0x0101010101010101ULL) >> 56;
}

int main(void) {
  uint64_t n = hide_u64(100000000);
  uint64_t state = hide_u64(7);
  uint64_t total = 0;
  for (uint64_t i = 0; i < n; ++i) {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    total += pop(state);
  }
  printf("%llu\n", (unsigned long long) total);
  return 0;
}
