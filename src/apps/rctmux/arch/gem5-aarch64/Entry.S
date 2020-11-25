#
# Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
# Economic rights: Technische Universitaet Dresden (Germany)
#
# This file is part of M3 (Microkernel for Minimalist Manycores).
#
# M3 is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# M3 is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License version 2 for more details.
#

#include <base/Asm.h>

#define BIT_SHIFT(attr, idx) ((attr) << (idx *8))
#define MT_DEVICE_nGnRnE    0
#define MT_DEVICE_nGnRE     1
#define MT_DEVICE_GRE       2
#define MT_NORMAL_NC        3
#define MT_NORMAL           4
#define MT_NORMAL_WT        5

.extern exc_handler
.extern init
.extern sleep
.global idle_stack

.text

BEGIN_FUNC(_start)
    // TODO currently required, because the reset lets us start at the next instruction
    nop

    mrs     mair_el1, x5 // We grab the NMRR:PRRR
    orr      x5,  x5, #0x00000002 // set tr0 = 0x2 (normal memory)
    orr      x5,  x5, BIT_SHIFT(#0x00000001, 4) // set ir0 = 0x1 (write-back, write-allocate)
    orr      x0,  x5, BIT_SHIFT(#0x00010000, 4) // set or0 = 0x1 (write-back, write-allocate)
    msr     mair_el1, x5 // Set it back into the register

    mrs     sctlr_el1,  x4 // We grab sctrl
    orr      x4,  x4, #0x00001000  // enable the I-cache
    orr      x4,  x4, #0x00000004     // enable D-cache
    orr      x4,  x4, #0x10000000     // enable TEX remap a lil suss
    msr     sctlr_el1,  x4

    // setup stack
    ldr     sp, =rctmux_stack
    mov     fp, #0

    // perform initialization, if necessary
    bl      init

    // enable interrupts and supervisor mode
    mov      x1, #0x1 // Set EL to el1
    msr     CurrentEL,  x1
    mov      x0, #0x0 // Enable interrupts
    msr     DAIF, x0

    // start idling
    ldr     lr, =idle
    ret
END_FUNC(_start)

BEGIN_FUNC(idle)
1:  bl      sleep
    b       1b
END_FUNC(idle)

.macro BUILD_ENTRY, no
BEGIN_FUNC(entry_\no)
    // temporarily save lr (old pc)
    ldr     sp, =_tmp
    str     lr, [sp]
    // save the vector number
    mov     lr, #\no
    str     lr, [sp, #8]
    // save SPSR (old CPRS)
    mrs     lr, SPSR_el1
    str     lr, [sp, #16]

    // switch back to supervisor mode
    //mrs     sp, CPSR
    //bic     sp, #0x1F
    //orr     sp, sp, #0x13
    //msr     CPSR, sp

.if \no == 6
    // use the application's stack; leave one slot for the pc
    sub     sp, sp, #16
.else
    // use our own stack
    ldr     sp, =rctmux_stack
.endif

    b       exc_common
END_FUNC(entry_\no)
.endm

BUILD_ENTRY 0
BUILD_ENTRY 1
BUILD_ENTRY 2
BUILD_ENTRY 3
BUILD_ENTRY 4
BUILD_ENTRY 6
BUILD_ENTRY 7

BEGIN_FUNC(exc_common)
    // save regs - x0- x29 this could have happened during pre-emption
    sub      sp,  sp, #30 * 8
    stp      x0,  x1, [sp, # 0 * 8]
    stp      x2,  x3, [sp, # 2 * 8]
    stp      x4,  x5, [sp, # 4 * 8]
    stp      x6,  x7, [sp, # 6 * 8]
    stp      x8,  x9, [sp, # 8 * 8]
    stp     x10, x11, [sp, #10 * 8]
    stp     x12, x13, [sp, #12 * 8]
    stp     x14, x15, [sp, #14 * 8]
    stp     x16, x17, [sp, #16 * 8]
    stp     x18, x19, [sp, #18 * 8]
    stp     x20, x21, [sp, #20 * 8]
    stp     x22, x23, [sp, #22 * 8]
    stp     x24, x25, [sp, #24 * 8]
    stp     x26, x27, [sp, #26 * 8]
    stp     x28, x29, [sp, #28 * 8]

    // save pc, cpsr and vector number
    ldr      x0, =_tmp
    ldp      x1, x2, [x0, #0 * 8]
    ldr      x3, [x0, #2 * 8]
    str      x1, [sp, #30 * 8]
    sub      sp,  sp, #16
    stp      x2,  x3, [sp, # 0 * 8]

    // call handler
    mov      x0,  sp
    bl      exc_handler

    // skip vector number
    //skip cpsr because we should have this saved in spsr
    add      sp,  x0, #16

    //Superseded by SPSR we think
    // restore CPSR
    //pop     {r0}
    //msr     CPSR, r0

    // restore register
    ldp      x0,  x1, [sp, # 0 * 8]
    ldp      x2,  x3, [sp, # 2 * 8]
    ldp      x4,  x5, [sp, # 4 * 8]
    ldp      x6,  x7, [sp, # 6 * 8]
    ldp      x8,  x9, [sp, # 8 * 8]
    ldp     x10, x11, [sp, #10 * 8]
    ldp     x12, x13, [sp, #12 * 8]
    ldp     x14, x15, [sp, #14 * 8]
    ldp     x16, x17, [sp, #16 * 8]
    ldp     x18, x19, [sp, #18 * 8]
    ldp     x20, x21, [sp, #20 * 8]
    ldp     x22, x23, [sp, #22 * 8]
    ldp     x24, x25, [sp, #24 * 8]
    ldp     x26, x27, [sp, #26 * 8]
    ldp     x28, x29, [sp, #28 * 8]

    add      sp,  sp, #30 * 8
    // continue execution
    eret
END_FUNC(exc_common)

.bss
_tmp:
    // temporary place for pc, cpsr and vector number
    .long       0
    .long       0
    .long       0
    .long       0
    .long       0
    .long       0

    .align     16
    .space 1024,0
rctmux_stack:
    .space 512, 0
idle_stack:
