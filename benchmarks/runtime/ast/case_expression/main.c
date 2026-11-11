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
  uint64_t state = hide_u64(3), acc = 0;
  for (uint64_t i = 0; i < n; ++i) {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    uint64_t v = state >> 61;
    switch (v) {
      case 0: acc = acc + 1; break;
      case 1: acc = acc + 3; break;
      case 2: acc = acc ^ 0x55; break;
      case 3: acc = acc + (state & 0xff); break;
      case 4: acc = acc - (acc >> 3); break;
      case 5: acc = acc + 7; break;
      case 6: acc = acc | 0x100; break;
      default: acc = acc + v; break;
    }
  }
  printf("%llu\n", (unsigned long long) acc);
  return 0;
}
