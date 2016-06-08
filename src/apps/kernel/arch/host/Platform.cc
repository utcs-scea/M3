/*
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <base/Config.h>
#include <base/Init.h>

#include "DTU.h"
#include "Platform.h"

namespace kernel {

INIT_PRIO_USER(2) Platform::KEnv Platform::_kenv;

Platform::KEnv::KEnv() {
    // no modules
    mods[0] = 0;

    // init PEs
    pe_count = MAX_CORES;
    for(int i = 0; i < MAX_CORES; ++i)
        pes[i] = m3::PE(m3::PEType::COMP_IMEM, 1024 * 1024);
}

size_t Platform::first_pe() {
    return 1;
}
size_t Platform::last_pe() {
    return _kenv.pe_count - 1;
}

uintptr_t Platform::def_recvbuf(size_t) {
    // unused
    return 0;
}

uintptr_t Platform::rw_barrier(size_t) {
    // no rw barrier here
    return 1;
}

}
