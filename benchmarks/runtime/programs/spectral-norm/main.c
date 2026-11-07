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

#include <math.h>

static double a(uint64_t i, uint64_t j) { return 1.0 / (double) ((i + j) * (i + j + 1) / 2 + i + 1); }

static void mul_av(const double *v, double *out, uint64_t n) {
  for (uint64_t i = 0; i < n; ++i) {
    double sum = 0.0;
    for (uint64_t j = 0; j < n; ++j) { sum = sum + a(i, j) * v[j]; }
    out[i] = sum;
  }
}

static void mul_atv(const double *v, double *out, uint64_t n) {
  for (uint64_t i = 0; i < n; ++i) {
    double sum = 0.0;
    for (uint64_t j = 0; j < n; ++j) { sum = sum + a(j, i) * v[j]; }
    out[i] = sum;
  }
}

int main(void) {
  uint64_t n = hide_u64(2000);
  double *u = malloc(n * sizeof(double)), *v = malloc(n * sizeof(double)), *tmp = malloc(n * sizeof(double));
  for (uint64_t i = 0; i < n; ++i) { u[i] = 1.0; v[i] = 0.0; tmp[i] = 0.0; }
  for (int round = 0; round < 10; ++round) {
    mul_av(u, tmp, n);
    mul_atv(tmp, v, n);
    mul_av(v, tmp, n);
    mul_atv(tmp, u, n);
  }
  double vbv = 0.0, vv = 0.0;
  for (uint64_t i = 0; i < n; ++i) {
    vbv = vbv + u[i] * v[i];
    vv = vv + v[i] * v[i];
  }
  free(u);
  free(v);
  free(tmp);
  printf("%lld\n", (long long) (sqrt(vbv / vv) * 1000000000.0));
  return 0;
}
