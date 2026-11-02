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
  size_t n = (size_t) hide_u64(700);
  int64_t *a = calloc(n * n, sizeof(int64_t)), *b = calloc(n * n, sizeof(int64_t)), *c = calloc(n * n, sizeof(int64_t));
  uint64_t state = hide_u64(99);
  for (size_t i = 0; i < n * n; ++i) {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    a[i] = (int64_t) ((state >> 33) % 100);
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    b[i] = (int64_t) ((state >> 33) % 100);
  }
  for (size_t i = 0; i < n; ++i) {
    for (size_t k = 0; k < n; ++k) {
      int64_t aik = a[i * n + k];
      for (size_t j = 0; j < n; ++j) { c[i * n + j] = c[i * n + j] + aik * b[k * n + j]; }
    }
  }
  int64_t total = 0;
  for (size_t i = 0; i < n * n; ++i) { total += c[i]; }
  free(a);
  free(b);
  free(c);
  printf("%lld\n", (long long) total);
  return 0;
}
