#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static size_t hide(size_t x) {
  volatile size_t v = x;
  return v;
}

int main(void) {
  size_t n = hide(20000000);
  bool *composite = calloc(n + 1, sizeof(bool));
  uint64_t count = 0;
  uint64_t sum = 0;
  for (size_t i = 2; i <= n; ++i) {
    if (!composite[i]) {
      count += 1;
      sum += i;
      for (size_t j = i * i; j <= n; j += i) { composite[j] = true; }
    }
  }
  free(composite);
  printf("%llu\n%llu\n", (unsigned long long) count, (unsigned long long) sum);
  return 0;
}
