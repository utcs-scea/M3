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

#pragma once

#include <base/PEDesc.h>

#include "ContextSwitcher.h"

namespace kernel {

class PEManager {
public:
    static void create() {
        _inst = new PEManager();
    }
    static PEManager &get() {
        return *_inst;
    }

private:
    explicit PEManager();

public:
    void init();

    peid_t find_pe(const m3::PEDesc &pe, bool tmuxable);
    void add_vpe(VPE *vpe);
    void start_vpe(VPE *vpe);
    void block_vpe(VPE *vpe);
    void unblock_vpe(VPE *vpe);
    void remove_vpe(VPE *vpe);

    void start_switch(peid_t pe);

private:
    void deprivilege_pes();

    static void continue_switch(ContextSwitcher *ctx);

    ContextSwitcher **_ctxswitcher;
    static PEManager *_inst;
};

}