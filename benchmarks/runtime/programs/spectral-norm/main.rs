use std::hint::black_box;

fn a(i: u64, j: u64) -> f64 {
    1.0 / ((i + j) * (i + j + 1) / 2 + i + 1) as f64
}

fn mul_av(v: &[f64], out: &mut [f64], n: u64) {
    for i in 0..n {
        let mut sum = 0.0;
        for j in 0..n { sum = sum + a(i, j) * v[j as usize]; }
        out[i as usize] = sum;
    }
}

fn mul_atv(v: &[f64], out: &mut [f64], n: u64) {
    for i in 0..n {
        let mut sum = 0.0;
        for j in 0..n { sum = sum + a(j, i) * v[j as usize]; }
        out[i as usize] = sum;
    }
}

fn main() {
    let n: u64 = black_box(2000);
    let len = n as usize;
    let mut u = vec![1.0f64; len];
    let mut v = vec![0.0f64; len];
    let mut tmp = vec![0.0f64; len];
    for _ in 0..10 {
        mul_av(&u, &mut tmp, n);
        mul_atv(&tmp, &mut v, n);
        mul_av(&v, &mut tmp, n);
        mul_atv(&tmp, &mut u, n);
    }
    let (mut vbv, mut vv) = (0.0f64, 0.0f64);
    for i in 0..len {
        vbv = vbv + u[i] * v[i];
        vv = vv + v[i] * v[i];
    }
    println!("{}", ((vbv / vv).sqrt() * 1000000000.0) as i64);
}
