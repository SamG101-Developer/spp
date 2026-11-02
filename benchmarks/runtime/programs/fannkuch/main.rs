use std::hint::black_box;

fn main() {
    let n: usize = black_box(10);
    let mut perm = [0i64; 16];
    let mut perm1 = [0i64; 16];
    let mut count = [0i64; 16];
    let (mut max_flips, mut checksum, mut perm_count) = (0i64, 0i64, 0i64);
    let mut r = n;
    for i in 0..n { perm1[i] = i as i64; }
    loop {
        while r != 1 {
            count[r - 1] = r as i64;
            r -= 1;
        }
        perm[..n].copy_from_slice(&perm1[..n]);
        let mut flips = 0i64;
        let mut k = perm[0] as usize;
        while k != 0 {
            let (mut lo, mut hi) = (0usize, k);
            while lo < hi {
                perm.swap(lo, hi);
                lo += 1;
                hi -= 1;
            }
            flips += 1;
            k = perm[0] as usize;
        }
        if flips > max_flips { max_flips = flips; }
        if perm_count % 2 == 0 { checksum += flips; } else { checksum -= flips; }
        loop {
            if r == n {
                println!("{checksum}\n{max_flips}");
                return;
            }
            let perm0 = perm1[0];
            for i in 0..r { perm1[i] = perm1[i + 1]; }
            perm1[r] = perm0;
            count[r] -= 1;
            if count[r] > 0 { break; }
            r += 1;
        }
        perm_count += 1;
    }
}
