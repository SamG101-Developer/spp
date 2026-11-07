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

typedef struct { int64_t x, y; } Point;

static Point step(Point p, int64_t k) {
  Point out = {(p.x * 3 + p.y + k) % 1000003, (p.y * 5 + p.x) % 999983};
  return out;
}

int main(void) {
  int64_t n = hide_s64(100000000);
  Point p = {hide_s64(1), hide_s64(2)};
  for (int64_t i = 0; i < n; ++i) { p = step(p, i % 7); }
  printf("%lld\n%lld\n", (long long) p.x, (long long) p.y);
  return 0;
}
