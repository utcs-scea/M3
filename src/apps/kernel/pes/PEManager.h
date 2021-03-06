/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

class VPEGroup;

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

    peid_t find_pe(const m3::PEDesc &pe, peid_t except, uint flags, const VPEGroup *group);

    bool can_unblock_now(VPE *vpe);
    VPE *current(peid_t pe) const;
    bool yield(peid_t pe);

    void add_vpe(VPE *vpe);
    void remove_vpe(VPE *vpe);

    void start_vpe(VPE *vpe);
    void stop_vpe(VPE *vpe);

    bool migrate_vpe(VPE *vpe, bool fast);
    bool migrate_for(VPE *vpe, VPE *dst);
    void yield_vpe(VPE *vpe);
    bool unblock_vpe(VPE *vpe, bool force);

private:
    bool migrate_to(VPE *vpe, peid_t npe, bool fast);
    void steal_vpe(peid_t pe);
    void update_yield(size_t before, size_t after);
    void deprivilege_pes();

    ContextSwitcher **_ctxswitcher;
    bool * _used;
    static PEManager *_inst;
};

}
