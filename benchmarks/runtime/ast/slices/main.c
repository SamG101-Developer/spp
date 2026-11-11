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
  size_t n = (size_t) hide_u64(4096);
  uint64_t *data = calloc(n, sizeof(uint64_t));
  uint64_t state = hide_u64(11);
  for (size_t i = 0; i < n; ++i) {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    data[i] = state >> 54;
  }
  uint64_t acc = 0;
  for (uint64_t round = 0; round < 25000; ++round) {
    for (size_t lo = 0; lo + 16 <= n; ++lo) {
      const uint64_t *window = data + lo;
      size_t len = 16;
      for (size_t j = 0; j < len; ++j) { acc = acc + window[j]; }
    }
  }
  free(data);
  printf("%llu\n", (unsigned long long) acc);
  return 0;
}
