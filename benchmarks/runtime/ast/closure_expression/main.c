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

typedef struct { uint64_t k; } Env;

static uint64_t step(const Env *env, uint64_t x) { return (x * env->k + 1) % 1000003; }

int main(void) {
  uint64_t n = hide_u64(100000000);
  Env env = {hide_u64(17)};
  uint64_t acc = hide_u64(5);
  for (uint64_t i = 0; i < n; ++i) { acc = step(&env, acc); }
  printf("%llu\n", (unsigned long long) acc);
  return 0;
}
