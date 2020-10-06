use time;

pub fn read8b(addr: usize) -> u64 {
    let res: u64;
    unsafe {
        asm!(
            "ldr $0, [$1]"
            : "=r"(res)
            : "r"(addr)
            : : "volatile"
        );
    }
    res
}

pub fn write8b(addr: usize, val: u64) {
    unsafe {
        asm!(
            "str $0, [$1]"
            : : "r"(val), "r"(addr)
            : : "volatile"
        );
    }
}

pub fn get_sp() -> usize {
    let res: usize;
    unsafe {
        asm!(
            "mov $0, x31;"
            : "=r"(res)
        );
    }
    res
}

pub fn get_bp() -> usize {
    let val: usize;
    unsafe {
        asm!(
            "mov $0, x29;"
            : "=r"(val)
        );
    }
    val
}

pub fn jmp_to(addr: usize) {
    unsafe {
        asm!(
            "mov pc, $0;"
            : : "r"(addr)
            : : "volatile"
        );
    }
}

pub fn gem5_debug(msg: usize) -> time::Time {
    let mut res = msg as time::Time;
    unsafe {
        asm!(
            ".long 0xEE630110"
            : "+{r0}"(res)
        );
    }
    res
}
