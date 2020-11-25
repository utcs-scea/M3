#
# Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
# Economic rights: Technische Universitaet Dresden (Germany)
#
# This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
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
#include <base/Config.h>

#define BIT_SHIFT(attr, idx) ((#attr) << (#idx *8))
.extern env_run

BEGIN_FUNC(_start)
    mrs      x5, mair_el1 // We grab the NMRR:PRRR
    orr      x5,  x5, #0x00000002 // set tr0 = 0x2 (normal memory)
    orr      x5,  x5, #0x0000000100000000 // set ir0 = 0x1 (write-back, write-allocate)
    orr      x0,  x5, #0x0001000000000000 // set or0 = 0x1 (write-back, write-allocate)
    msr     mair_el1, x5 // Set it back into the register

    mrs      x4, sctlr_el1 // We grab sctrl
    orr      x4,  x4, #0x00001000  // enable the I-cache
    orr      x4,  x4, #0x00000004     // enable D-cache
    orr      x4,  x4, #0x10000000     // enable TEX remap a lil suss
    msr     sctlr_el1, x4

    // set default stack pointer?
    mov      x0, #0xDEADBEE0
    cmp      x1,  x0
    beq      1f
    mov      sp, #(STACK_TOP - 16)

1:
    mov      fp, #0
    bl      env_run

    // just to be sure
1:  b       1b
END_FUNC(_start)
