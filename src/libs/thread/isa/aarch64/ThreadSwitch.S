#include <base/Asm.h>

#include <thread/isa/arm/Thread.h>

# bool thread_save(m3::Thread::Regs *regs);
BEGIN_FUNC(thread_save)
    mov      x1,  #1 // thread_resume's return value
    mov      x3,  sp
    stp     x30,  x3, [x0, #12 * 8]
    stp     x28, x29, [x0, #10 * 8]
    stp     x26, x27, [x0, #8 * 8]
    stp     x24, x25, [x0, #6 * 8]
    stp     x22, x23, [x0, #4 * 8]
    stp     x20, x21, [x0, #2 * 8]
    stp      x1, x19, [x0, #0 * 8]
    mrs      x1,DAIF
    mrs      x2,NZCV
    stp      x1,  x2, [x0, #14 * 8]
    mov      x0,  #0

    ret
END_FUNC(thread_save)

# bool thread_resume(m3::Thread::Regs *regs);
BEGIN_FUNC(thread_resume)
    ldp       x1,  x2, [x0, #14 * 8]
    msr     DAIF,  x1
    msr     NZCV,  x2
    ldp      x30,  x3, [x0, #12 * 8]
    mov       sp,  x3
    ldp      x28, x29, [x0, #10 * 8]
    ldp      x26, x27, [x0, #8 * 8]
    ldp      x24, x25, [x0, #6 * 8]
    ldp      x22, x23, [x0, #4 * 8]
    ldp      x20, x21, [x0, #2 * 8]
    ldp       x0, x19, [x0, #0 * 8]

    ret
END_FUNC(thread_resume)
