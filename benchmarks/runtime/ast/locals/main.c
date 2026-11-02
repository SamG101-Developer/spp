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

typedef struct { uint64_t a, b; } Pair;

int main(void) {
  uint64_t n = hide_u64(100000000);
  uint64_t x = hide_u64(1), y = hide_u64(2);
  for (uint64_t i = 0; i < n; ++i) {
    uint64_t t;
    t = (x + y) % 1000003;
    uint64_t p = t, q = x;
    uint64_t arr[3] = {q, p, y};
    uint64_t r = arr[0], s = arr[2];
    Pair pair = {r, s};
    uint64_t a = pair.a, b = pair.b;
    uint64_t z;
    {
      uint64_t w = a * 3 + b;
      z = w % 999983;
    }
    {
      x = z;
    }
    y = (y + i) % 1000003;
  }
  printf("%llu\n%llu\n", (unsigned long long) x, (unsigned long long) y);
  return 0;
}
