#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t hide_u64(uint64_t x) {
  volatile uint64_t v = x;
  return v;
}

static double hide_f64(double x) {
  volatile double v = x;
  return v;
}

static uint64_t bits(double x) {
  uint64_t u;
  memcpy(&u, &x, sizeof u);
  return u;
}

int main(void) {
  uint64_t n = hide_u64(4096);
  int64_t passes = (int64_t) hide_u64(25000);
  double *a = malloc(n * sizeof *a);
  double *b = malloc(n * sizeof *b);
  double fill_a = hide_f64(0.5);
  double fill_b = hide_f64(0.5);
  for (uint64_t i = 0; i < n; ++i) { a[i] = fill_a; b[i] = fill_b; }
  for (int64_t pass = 0; pass < passes; ++pass) {
    for (uint64_t i = 0; i < n; ++i) { a[i] = a[i] * b[i] + 1.0; }
  }
  printf("%llu\n", (unsigned long long) (bits(a[0]) + bits(a[4095])));
  free(a);
  free(b);
  return 0;
}
