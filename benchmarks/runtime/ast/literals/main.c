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

#include <string.h>

int main(void) {
  uint64_t n = hide_u64(100000000);
  uint64_t acc = 0;
  double f = 0.0;
  for (uint64_t i = 0; i < n; ++i) {
    const uint64_t small[8] = {1, 2, 3, 5, 8, 13, 21, 34};
    uint64_t window[4] = {0};
    window[i % 4] = i;
    uint64_t k = i % 7;
    bool flag = true;
    char c = 'x';
    const char *word = "benchmark";
    acc = acc + small[i % 8] + window[0] + (k * 3) + strlen(word);
    if (flag && c == 'x') { acc += 1; }
    f = f + 0.5;
  }
  printf("%llu\n%lld\n", (unsigned long long) acc, (long long) f);
  return 0;
}
