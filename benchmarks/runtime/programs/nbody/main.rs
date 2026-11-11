use std::hint::black_box;

fn energy(x: &[f64; 5], y: &[f64; 5], z: &[f64; 5], vx: &[f64; 5], vy: &[f64; 5], vz: &[f64; 5], m: &[f64; 5]) -> f64 {
    let mut e = 0.0;
    for i in 0..5 {
        e = e + 0.5 * m[i] * (vx[i] * vx[i] + vy[i] * vy[i] + vz[i] * vz[i]);
        for j in i + 1..5 {
            let (dx, dy, dz) = (x[i] - x[j], y[i] - y[j], z[i] - z[j]);
            e = e - m[i] * m[j] / (dx * dx + dy * dy + dz * dz).sqrt();
        }
    }
    e
}

fn main() {
    let steps: u64 = black_box(5_000_000);
    let pi = 3.141592653589793f64;
    let solar = 4.0 * pi * pi;
    let days = 365.24f64;
    let dt: f64 = black_box(0.01);
    let mut x = [0.0, 4.84143144246472090, 8.34336671824457987, 12.8943695621391310, 15.3796971148509165];
    let mut y = [0.0, -1.16032004402742839, 4.12479856412430479, -15.1111514016986312, -25.9193146099879641];
    let mut z = [0.0, -0.103622044471123109, -0.403523417114321381, -0.223307578892655734, 0.179258772950371181];
    let mut vx = [0.0, 0.00166007664274403694 * days, -0.00276742510726862411 * days, 0.00296460137564761618 * days,
        0.00268067772490389322 * days];
    let mut vy = [0.0, 0.00769901118419740425 * days, 0.00499852801234917238 * days, 0.00237847173959480950 * days,
        0.00162824170038242295 * days];
    let mut vz = [0.0, -0.0000690460016972063023 * days, 0.0000230417297573763929 * days,
        -0.0000296589568540237556 * days, -0.0000951592254519715870 * days];
    let m = [solar, 0.000954791938424326609 * solar, 0.000285885980666130812 * solar,
        0.0000436624404335156298 * solar, 0.0000515138902046611451 * solar];

    let (mut px, mut py, mut pz) = (0.0f64, 0.0f64, 0.0f64);
    for i in 0..5 {
        px = px + vx[i] * m[i];
        py = py + vy[i] * m[i];
        pz = pz + vz[i] * m[i];
    }
    vx[0] = 0.0 - px / solar;
    vy[0] = 0.0 - py / solar;
    vz[0] = 0.0 - pz / solar;
    println!("{}", (energy(&x, &y, &z, &vx, &vy, &vz, &m) * 1000000000.0) as i64);

    for _ in 0..steps {
        for i in 0..5 {
            for j in i + 1..5 {
                let (dx, dy, dz) = (x[i] - x[j], y[i] - y[j], z[i] - z[j]);
                let d2 = dx * dx + dy * dy + dz * dz;
                let mag = dt / (d2 * d2.sqrt());
                vx[i] = vx[i] - dx * m[j] * mag;
                vy[i] = vy[i] - dy * m[j] * mag;
                vz[i] = vz[i] - dz * m[j] * mag;
                vx[j] = vx[j] + dx * m[i] * mag;
                vy[j] = vy[j] + dy * m[i] * mag;
                vz[j] = vz[j] + dz * m[i] * mag;
            }
        }
        for i in 0..5 {
            x[i] = x[i] + dt * vx[i];
            y[i] = y[i] + dt * vy[i];
            z[i] = z[i] + dt * vz[i];
        }
    }
    println!("{}", (energy(&x, &y, &z, &vx, &vy, &vz, &m) * 1000000000.0) as i64);
}
