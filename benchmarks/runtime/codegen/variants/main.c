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

typedef struct { bool some; uint64_t value; } Opt;

static Opt find(uint64_t x) {
  if (x % 5 == 0) { return (Opt) {false, 0}; }
  return (Opt) {true, x * 2};
}

int main(void) {
  uint64_t n = hide_u64(100000000);
  uint64_t acc = 0;
  for (uint64_t i = 0; i < n; ++i) {
    Opt found = find(i);
    if (found.some) { acc = (acc + found.value) % 1000000007; } else { acc += 1; }
  }
  printf("%llu\n", (unsigned long long) acc);
  return 0;
}
