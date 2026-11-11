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

__attribute__((noinline)) static uint64_t apply(uint64_t (*f)(uint64_t), uint64_t x) { return f(x); }
static uint64_t triple(uint64_t x) { return (x * 3 + 1) % 1000003; }
static uint64_t halve(uint64_t x) { return x / 2 + 7; }
typedef struct { uint64_t side; } Square;
static uint64_t area(const Square *self, uint64_t k) { return self->side * self->side + k; }

int main(void) {
  uint64_t n = hide_u64(50000000);
  uint64_t acc = hide_u64(1);
  for (uint64_t i = 0; i < n; ++i) {
    acc = apply(triple, acc);
    acc = apply(halve, acc);
    Square sq = {i % 100};
    acc = (acc + area(&sq, 3)) % 1000003;
  }
  printf("%llu\n", (unsigned long long) acc);
  return 0;
}
