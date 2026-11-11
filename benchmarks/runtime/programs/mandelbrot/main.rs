use std::hint::black_box;

fn main() {
    let size: u64 = black_box(1800);
    let max_iter: u64 = black_box(50);
    let scale = size as f64;
    let mut inside = 0u64;
    let mut y = 0u64;
    while y < size {
        let ci = y as f64 * 2.0 / scale - 1.0;
        let mut x = 0u64;
        while x < size {
            let cr = x as f64 * 2.0 / scale - 1.5;
            let (mut zr, mut zi) = (0.0f64, 0.0f64);
            let mut i = 0u64;
            let mut escaped = false;
            while i < max_iter && !escaped {
                let tr = zr * zr - zi * zi + cr;
                zi = 2.0 * zr * zi + ci;
                zr = tr;
                if zr * zr + zi * zi > 4.0 { escaped = true; }
                i += 1;
            }
            if !escaped { inside += 1; }
            x += 1;
        }
        y += 1;
    }
    println!("{inside}");
}
