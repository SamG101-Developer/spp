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
  uint64_t n = hide_u64(200000000);
  uint64_t state = hide_u64(12345);
  uint64_t hash = 0xcbf29ce484222325ULL;
  for (uint64_t i = 0; i < n; ++i) {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    hash = (hash ^ (state >> 56)) * 0x100000001b3ULL;
  }
  printf("%llu\n", (unsigned long long) hash);
  return 0;
}
