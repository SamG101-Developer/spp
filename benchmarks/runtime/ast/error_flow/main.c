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

typedef struct { bool ok; uint64_t value; } Res;

static Res check(uint64_t x) {
  if (!(x % 97 != 0)) { return (Res) {false, x}; }
  return (Res) {true, x * 3 % 1000003};
}

static Res chain(uint64_t x) {
  Res a = check(x);
  if (!a.ok) { return a; }
  Res b = check(a.value + 1);
  if (!b.ok) { return b; }
  return (Res) {true, a.value + b.value};
}

int main(void) {
  uint64_t n = hide_u64(50000000);
  uint64_t ok = 0, failed = 0;
  for (uint64_t i = 0; i < n; ++i) {
    Res r = chain(i);
    if (r.ok) { ok = (ok + r.value) % 1000000007; } else { failed += 1; }
  }
  printf("%llu\n%llu\n", (unsigned long long) ok, (unsigned long long) failed);
  return 0;
}
