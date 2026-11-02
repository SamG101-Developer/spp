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
  uint64_t acc = 0;
  for (uint64_t i = 0; i < n; ++i) {
    uint64_t *v = malloc(8 * sizeof(uint64_t));
    size_t len = 0;
    v[len++] = i;
    v[len++] = i % 13;
    acc = (acc + v[0] + v[1]) % 1000000007;
    free(v);
  }
  printf("%llu\n", (unsigned long long) acc);
  return 0;
}
