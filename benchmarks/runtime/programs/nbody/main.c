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

static double energy(const double *x, const double *y, const double *z, const double *vx, const double *vy,
                     const double *vz, const double *m) {
  double e = 0.0;
  for (size_t i = 0; i < 5; ++i) {
    e = e + 0.5 * m[i] * (vx[i] * vx[i] + vy[i] * vy[i] + vz[i] * vz[i]);
    for (size_t j = i + 1; j < 5; ++j) {
      double dx = x[i] - x[j], dy = y[i] - y[j], dz = z[i] - z[j];
      e = e - m[i] * m[j] / sqrt(dx * dx + dy * dy + dz * dz);
    }
  }
  return e;
}

int main(void) {
  uint64_t steps = hide_u64(5000000);
  double pi = 3.141592653589793;
  double solar = 4.0 * pi * pi;
  double days = 365.24;
  double dt = hide_f64(0.01);
  double x[5] = {0.0, 4.84143144246472090, 8.34336671824457987, 12.8943695621391310, 15.3796971148509165};
  double y[5] = {0.0, -1.16032004402742839, 4.12479856412430479, -15.1111514016986312, -25.9193146099879641};
  double z[5] = {0.0, -0.103622044471123109, -0.403523417114321381, -0.223307578892655734, 0.179258772950371181};
  double vx[5] = {0.0, 0.00166007664274403694 * days, -0.00276742510726862411 * days, 0.00296460137564761618 * days,
                  0.00268067772490389322 * days};
  double vy[5] = {0.0, 0.00769901118419740425 * days, 0.00499852801234917238 * days, 0.00237847173959480950 * days,
                  0.00162824170038242295 * days};
  double vz[5] = {0.0, -0.0000690460016972063023 * days, 0.0000230417297573763929 * days,
                  -0.0000296589568540237556 * days, -0.0000951592254519715870 * days};
  double m[5] = {solar, 0.000954791938424326609 * solar, 0.000285885980666130812 * solar,
                 0.0000436624404335156298 * solar, 0.0000515138902046611451 * solar};

  double px = 0.0, py = 0.0, pz = 0.0;
  for (size_t i = 0; i < 5; ++i) {
    px = px + vx[i] * m[i];
    py = py + vy[i] * m[i];
    pz = pz + vz[i] * m[i];
  }
  vx[0] = 0.0 - px / solar;
  vy[0] = 0.0 - py / solar;
  vz[0] = 0.0 - pz / solar;
  printf("%lld\n", (long long) (energy(x, y, z, vx, vy, vz, m) * 1000000000.0));

  for (uint64_t step = 0; step < steps; ++step) {
    for (size_t i = 0; i < 5; ++i) {
      for (size_t j = i + 1; j < 5; ++j) {
        double dx = x[i] - x[j], dy = y[i] - y[j], dz = z[i] - z[j];
        double d2 = dx * dx + dy * dy + dz * dz;
        double mag = dt / (d2 * sqrt(d2));
        vx[i] = vx[i] - dx * m[j] * mag;
        vy[i] = vy[i] - dy * m[j] * mag;
        vz[i] = vz[i] - dz * m[j] * mag;
        vx[j] = vx[j] + dx * m[i] * mag;
        vy[j] = vy[j] + dy * m[i] * mag;
        vz[j] = vz[j] + dz * m[i] * mag;
      }
    }
    for (size_t i = 0; i < 5; ++i) {
      x[i] = x[i] + dt * vx[i];
      y[i] = y[i] + dt * vy[i];
      z[i] = z[i] + dt * vz[i];
    }
  }
  printf("%lld\n", (long long) (energy(x, y, z, vx, vy, vz, m) * 1000000000.0));
  return 0;
}
