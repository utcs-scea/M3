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

#include <base/col/SList.h>
#include <base/KIF.h>

#include <m3/com/SendGate.h>
#include <m3/VPE.h>

#include <fs/internal.h>

#include "Session.h"

class M3FSMetaSession;

struct CapContainer {
    struct Entry : public m3::SListItem {
        explicit Entry(capsel_t _sel) : sel(_sel) {
        }
        ~Entry() {
            m3::VPE::self().revoke(m3::KIF::CapRngDesc(m3::KIF::CapRngDesc::OBJ, sel));
        }

        capsel_t sel;
    };

    explicit CapContainer() : caps() {
    }
    ~CapContainer() {
        for(auto it = caps.begin(); it != caps.end(); ) {
            auto old = it++;
            delete &*old;
        }
    }

    void add(capsel_t sel) {
        caps.append(new Entry(sel));
    }

    m3::SList<Entry> caps;
};

class M3FSFileSession : public M3FSSession, public m3::SListItem {
public:
    explicit M3FSFileSession(capsel_t srv, M3FSMetaSession *meta, const m3::String &filename,
                             int flags, m3::inodeno_t ino);
    virtual ~M3FSFileSession();

    virtual Type type() const override {
        return FILE;
    }

    virtual void read(m3::GateIStream &is) override;
    virtual void write(m3::GateIStream &is) override;
    virtual void seek(m3::GateIStream &is) override;
    virtual void fstat(m3::GateIStream &is) override;

    m3::inodeno_t ino() const {
        return _ino;
    }

    m3::KIF::CapRngDesc caps() const {
        return m3::KIF::CapRngDesc(m3::KIF::CapRngDesc::OBJ, _sess, 2);
    }
    void set_ep(capsel_t ep) {
        _epcap = ep;
    }

    m3::Errors::Code clone(capsel_t srv, m3::KIF::Service::ExchangeData &data);
    m3::Errors::Code get_mem(m3::KIF::Service::ExchangeData &data);

private:
    void read_write(m3::GateIStream &is, bool write);
    m3::Errors::Code commit(m3::INode *inode, size_t submit);

    size_t _extent;
    size_t _extoff;
    size_t _lastoff;
    size_t _extlen;
    size_t _fileoff;

    bool _appending;
    m3::Extent *_append_ext;

    capsel_t _last;
    capsel_t _epcap;
    capsel_t _sess;
    m3::SendGate _sgate;

    int _oflags;
    m3::String _filename;
    m3::inodeno_t _ino;

    CapContainer _capscon;
    M3FSMetaSession *_meta;
};
