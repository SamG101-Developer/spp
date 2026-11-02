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
  uint64_t size = hide_u64(1800);
  uint64_t max_iter = hide_u64(50);
  double scale = (double) size;
  uint64_t inside = 0;
  for (uint64_t y = 0; y < size; ++y) {
    double ci = (double) y * 2.0 / scale - 1.0;
    for (uint64_t x = 0; x < size; ++x) {
      double cr = (double) x * 2.0 / scale - 1.5;
      double zr = 0.0, zi = 0.0;
      uint64_t i = 0;
      bool escaped = false;
      while (i < max_iter && !escaped) {
        double tr = zr * zr - zi * zi + cr;
        zi = 2.0 * zr * zi + ci;
        zr = tr;
        if (zr * zr + zi * zi > 4.0) { escaped = true; }
        i += 1;
      }
      if (!escaped) { inside += 1; }
    }
  }
  printf("%llu\n", (unsigned long long) inside);
  return 0;
}
