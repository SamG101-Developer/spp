use std::hint::black_box;

fn emit(code: &mut Vec<u64>, op: u64, a: u64, b: u64) {
    code.push(op);
    code.push(a);
    code.push(b);
}

fn main() {
    let n: u64 = black_box(30_000_000);
    let mut code = Vec::new();
    emit(&mut code, 1, 0, n);
    emit(&mut code, 1, 1, 0);
    emit(&mut code, 1, 3, 85);
    emit(&mut code, 2, 1, 0);
    emit(&mut code, 3, 1, 3);
    emit(&mut code, 4, 0, 1);
    emit(&mut code, 5, 0, 3);
    emit(&mut code, 0, 0, 0);

    let mut regs = [0u64; 4];
    let mut pc = 0usize;
    let mut running = true;
    while running {
        let op = code[pc * 3];
        let a = code[pc * 3 + 1] as usize;
        let b = code[pc * 3 + 2];
        pc += 1;
        match op {
            0 => running = false,
            1 => regs[a] = b,
            2 => regs[a] = regs[a] + regs[b as usize],
            3 => regs[a] = regs[a] ^ regs[b as usize],
            4 => regs[a] = regs[a] - b,
            5 => {
                if regs[a] != 0 { pc = b as usize; }
            }
            _ => running = false,
        }
    }
    println!("{}", regs[1]);
}
