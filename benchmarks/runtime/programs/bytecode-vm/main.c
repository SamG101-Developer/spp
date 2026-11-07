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

typedef struct { uint64_t *data; size_t len, cap; } Code;

static void emit(Code *code, uint64_t op, uint64_t a, uint64_t b) {
  if (code->len + 3 > code->cap) {
    code->cap = code->cap ? code->cap * 2 : 8;
    code->data = realloc(code->data, code->cap * sizeof(uint64_t));
  }
  code->data[code->len++] = op;
  code->data[code->len++] = a;
  code->data[code->len++] = b;
}

int main(void) {
  uint64_t n = hide_u64(30000000);
  Code code = {0};
  emit(&code, 1, 0, n);
  emit(&code, 1, 1, 0);
  emit(&code, 1, 3, 85);
  emit(&code, 2, 1, 0);
  emit(&code, 3, 1, 3);
  emit(&code, 4, 0, 1);
  emit(&code, 5, 0, 3);
  emit(&code, 0, 0, 0);

  uint64_t regs[4] = {0};
  size_t pc = 0;
  bool running = true;
  while (running) {
    uint64_t op = code.data[pc * 3];
    size_t a = (size_t) code.data[pc * 3 + 1];
    uint64_t b = code.data[pc * 3 + 2];
    pc += 1;
    switch (op) {
      case 0: running = false; break;
      case 1: regs[a] = b; break;
      case 2: regs[a] = regs[a] + regs[b]; break;
      case 3: regs[a] = regs[a] ^ regs[b]; break;
      case 4: regs[a] = regs[a] - b; break;
      case 5: if (regs[a] != 0) { pc = (size_t) b; } break;
      default: running = false; break;
    }
  }
  free(code.data);
  printf("%llu\n", (unsigned long long) regs[1]);
  return 0;
}
