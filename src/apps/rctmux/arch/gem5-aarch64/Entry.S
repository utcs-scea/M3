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
    // save SPRS (old CPRS)
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
    // save regs - callee saved and x0, x1, x2, x3
    push    {r0-r12, lr}

    // save pc, cpsr and vector number
    ldr     x0, =_tmp
    ldp     r0, {r1, r2, r3}
    str     r1, [sp, #14 * 4]
    sub     sp, sp, #8
    stm     sp, {r2, r3}

    // call handler
    mov     r0, sp
    bl      exc_handler

    // skip vector number
    add     sp, r0, #4

    //Superseded by SPSR we think
    // restore CPSR
    //pop     {r0}
    //msr     CPSR, r0

    // restore register
    pop     {r0-r12, lr}
    // continue execution
    eret
END_FUNC(exc_common)

.bss
_tmp:
    // temporary place for pc, cpsr and vector number
    .long       0
    .long       0
    .long       0

    .align      2
    .space 1024,0
rctmux_stack:
    .space 256, 0
idle_stack:
