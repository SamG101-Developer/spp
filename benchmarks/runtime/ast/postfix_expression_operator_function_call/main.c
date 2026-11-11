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

static uint64_t fib(uint64_t n) {
  if (n < 2) { return n; }
  return fib(n - 1) + fib(n - 2);
}

int main(void) {
  printf("%llu\n", (unsigned long long) fib(hide_u64(39)));
  return 0;
}
