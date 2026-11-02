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

typedef struct { uint64_t total, step; } Counter;

static Counter make(uint64_t step) { return (Counter) {0, step}; }
static void advance(Counter *self, uint64_t by) { self->total = (self->total + by * self->step) % 1000003; }
static uint64_t peek(const Counter *self) { return self->total; }
static void add_into(uint64_t *target, uint64_t value) { *target = (*target + value) % 1000000007; }

int main(void) {
  uint64_t n = hide_u64(100000000);
  Counter c = make(hide_u64(7));
  uint64_t acc = 0;
  for (uint64_t i = 0; i < n; ++i) {
    advance(&c, i);
    add_into(&acc, peek(&c));
  }
  printf("%llu\n", (unsigned long long) acc);
  return 0;
}
