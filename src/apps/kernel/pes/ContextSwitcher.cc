/*
 * Copyright (C) 2015, René Küttner <rene.kuettner@tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <base/RCTMux.h>
#include <base/log/Kernel.h>
#include <base/col/Treap.h>

#include "DTU.h"
#include "pes/ContextSwitcher.h"
#include "pes/Timeouts.h"
#include "pes/VPEManager.h"
#include "pes/VPE.h"
#include "Platform.h"
#include "WorkLoop.h"

namespace kernel {

static const char *stateNames[] = {
    "S_IDLE",
    "S_STORE_WAIT",
    "S_STORE_DONE",
    "S_SWITCH",
    "S_RESTORE_WAIT",
    "S_RESTORE_DONE",
};

/**
 * The state machine for context switching looks as follows:
 *
 *          switch & cur     +----------+
 *         /-----------------|  S_IDLE  |<--------------\
 *         |                 +----------+               |
 *         v                     |   |                  |
 * +------------------+          |   |         +-----------------+
 * |   S_STORE_WAIT   |   switch |   |         |  S_RESTORE_DONE |
 * |   ------------   |     &    |   |         |  -------------- |
 * |   e/ inject IRQ  |    !cur  |   |         |    e/ notify    |
 * +------------------+          |   | start   +-----------------+
 *         |                     |   |                  ^
 *         | signal              |   |                  | signal
 *         |                     |   |                  |
 *         v                     |   |                  |
 * +------------------+          |   |         +-----------------+
 * |   S_STORE_DONE   |          |   |         |  S_RESTORE_WAIT |
 * |   ------------   |          |   \-------->|  -------------- |
 * | e/ save DTU regs |          |             |    e/ wakeup    |
 * +------------------+          |             +-----------------+
 *         |                     v                      ^
 *         |             +------------------+           |
 *         |             |     S_SWITCH     |           |
 *         \------------>|     --------     |-----------/
 *                       | e/ sched & reset |
 *                       +------------------+
 */

ContextSwitcher::ContextSwitcher(size_t pe)
    : _pe(pe), _state(S_IDLE), _count(), _ready(), _it(),
      _timeout(), _wait_time(), _idle(), _cur() {
    assert(pe > 0);
    KLOG(VPES, "Initialized context switcher for pe " << pe);
}

bool ContextSwitcher::can_mux() const {
    for(auto it = _ready.begin(); it != _ready.end(); ++it) {
        if(!(it->flags() & VPE::F_MUXABLE))
            return false;
    }
    return true;
}

void ContextSwitcher::send_flags(vpeid_t vpeid, uint64_t flags) {
    alignas(DTU_PKG_SIZE) uint64_t ctrl = flags;
    DTU::get().write_mem(VPEDesc(_pe, vpeid), RCTMUX_FLAGS, &ctrl, sizeof(ctrl));
}

void ContextSwitcher::recv_flags(vpeid_t vpeid, uint64_t *flags) {
    DTU::get().read_mem(VPEDesc(_pe, vpeid), RCTMUX_FLAGS, flags, sizeof(*flags));
}

VPE* ContextSwitcher::schedule() {
    if (_ready.length() > 0) {
        _it++;
        if (_it == _ready.end())
            _it = _ready.begin();
        return &*_it;
    }

    return _idle;
}

void ContextSwitcher::init() {
    assert(_idle == nullptr);

    _idle = new VPE(m3::String("idle"), _pe, VPEManager::get().get_id(),
        VPE::F_IDLE | VPE::F_INIT | VPE::F_BOOTMOD, -1, m3::KIF::INV_SEL);
}

void ContextSwitcher::enqueue(VPE *vpe) {
    if(vpe->_flags & VPE::F_READY)
        return;

    vpe->_flags |= VPE::F_READY;
    _ready.append(vpe);
    if(_ready.length() == 1)
        _it = _ready.begin();
}

void ContextSwitcher::dequeue(VPE *vpe) {
    if(!(vpe->_flags & VPE::F_READY))
        return;

    vpe->_flags &= ~VPE::F_READY;
    _ready.remove(vpe);
    if(&*_it == vpe)
        _it = _ready.begin();
}

void ContextSwitcher::add(VPE *vpe) {
    _count++;
    unblock_vpe(vpe);
}

void ContextSwitcher::remove(VPE *vpe, bool destroy) {
    dequeue(vpe);
    _count--;

    if(_cur == vpe) {
        if(destroy) {
            _cur->_state = VPE::DEAD;
            // the VPE id is expected to be invalid in S_SWITCH
            DTU::get().unset_vpeid(_cur->desc());
        }
        _cur = nullptr;
        start_switch();
    }
}

void ContextSwitcher::block_vpe(VPE *vpe) {
    dequeue(vpe);
    start_switch();
}

void ContextSwitcher::unblock_vpe(VPE *vpe) {
    enqueue(vpe);
    // TODO don't do that immediately
    start_switch();
}

void ContextSwitcher::start_switch(bool timedout) {
    if(!timedout && _timeout)
        Timeouts::get().cancel(_timeout);
    _timeout = nullptr;

    // if there is a switch running, do nothing
    if(_state != S_IDLE)
        return;

    // if no VPE is running, directly switch to a new VPE
    if (_cur == nullptr)
        _state = S_SWITCH;
    else
        _state = S_STORE_WAIT;

    next_state(0);
}

void ContextSwitcher::start_vpe() {
    assert(_state == S_IDLE);
    assert(_cur);
    assert(_cur->state() == VPE::RUNNING);
    assert(_cur->flags() & VPE::F_START);

    _state = S_RESTORE_WAIT;
    next_state(0);
}

void ContextSwitcher::continue_switch() {
    assert(_state == S_STORE_DONE || _state == S_RESTORE_DONE);

    uint64_t flags = 0;
    // rctmux is expected to invalidate the VPE id after we've injected the IRQ
    recv_flags(_state == S_STORE_DONE ? VPE::INVALID_ID : _cur->id(), &flags);

    if(~flags & m3::RCTMuxCtrl::SIGNAL) {
        assert(_wait_time > 0);
        if(_wait_time < MAX_WAIT_TIME)
            _wait_time *= 2;
        Timeouts::get().wait_for(_wait_time, std::bind(&ContextSwitcher::continue_switch, this));
    }
    else
        next_state(flags);
}

void ContextSwitcher::next_state(uint64_t flags) {
    KLOG(VPES, "CtxSw[" << _pe << "]: next; state=" << stateNames[static_cast<size_t>(_state)]
        << " (current=" << (_cur ? _cur->id() : 0) << ":"
                        << (_cur ? _cur->name().c_str() : "-") << ")");

    _wait_time = 0;
    switch(_state) {
        case S_IDLE:
            assert(false);
            break;

        case S_STORE_WAIT: {
            send_flags(_cur->id(), m3::RCTMuxCtrl::STORE);
            DTU::get().injectIRQ(_cur->desc());

            _state = S_STORE_DONE;

            _wait_time = INIT_WAIT_TIME;
            Timeouts::get().wait_for(_wait_time, std::bind(&ContextSwitcher::continue_switch, this));
            break;
        }

        case S_STORE_DONE: {
            _cur->_dtustate.save(_cur->desc());

            uint64_t now = DTU::get().get_time();
            uint64_t cycles = _cur->_dtustate.get_idle_time();
            uint64_t total = now - _cur->_lastsched;
            KLOG(VPES, "CtxSw[" << _pe << "]: VPE idled for " << cycles << " of " << total
                << " cycles (now=" << now << ", last=" << _cur->_lastsched << ")");
            KLOG(VPES, "CtxSw[" << _pe << "]: VPE state can be set to "
                << ((flags & m3::RCTMuxCtrl::BLOCK) ? "blocked" : "ready"));

            _cur->_state = VPE::SUSPENDED;
            if(flags & m3::RCTMuxCtrl::BLOCK)
                dequeue(_cur);
            // ensure that it is still enqueued. the idle syscall might have dequeued it
            // note that we want to make it ready even in this case, because that means that, e.g.,
            // the PE has received a message and thus does not want to be blocked anymore
            else
                enqueue(_cur);

            // fall through
        }

        case S_SWITCH: {
            _cur = schedule();

            // make it running here, so that the PTEs are sent to the PE, if F_INIT is set
            _cur->_state = VPE::RUNNING;
            _cur->_lastsched = DTU::get().get_time();

            _cur->_dtustate.reset(RCTMUX_ENTRY);

            VPEDesc vpe(_pe, VPE::INVALID_ID);
            _cur->_dtustate.restore(vpe, _cur->id());

            if(_cur->flags() & VPE::F_INIT)
                _cur->init_memory();
            if((_cur->flags() & (VPE::F_BOOTMOD | VPE::F_START)) == (VPE::F_BOOTMOD | VPE::F_START))
                _cur->load_app(_cur->name().c_str());

            // fall through
        }

        case S_RESTORE_WAIT: {
            uint64_t flags = 0;
            // it's the first start if we are initializing or starting
            if(_cur->flags() & (VPE::F_INIT | VPE::F_START))
                flags |= m3::RCTMuxCtrl::INIT;

            // there is an application to restore if we are either resuming an application (!INIT)
            // or if we are just starting it
            if(!(_cur->flags() & VPE::F_INIT) || (_cur->flags() & VPE::F_START))
                flags |= m3::RCTMuxCtrl::RESTORE | (static_cast<uint64_t>(_pe) << 32);

            // let the VPE report idle times if there are other VPEs on this PE
            if(_ready.length() > 1)
                flags |= m3::RCTMuxCtrl::REPORT;

            KLOG(VPES, "CtxSw[" << _pe << "]: waking up PE with flags=" << m3::fmt(flags, "#x"));

            send_flags(_cur->id(), flags);
            DTU::get().wakeup(_cur->desc());
            _state = S_RESTORE_DONE;

            _wait_time = INIT_WAIT_TIME;
            Timeouts::get().wait_for(_wait_time, std::bind(&ContextSwitcher::continue_switch, this));
            break;
        }

        case S_RESTORE_DONE: {
            // we have finished these phases now (if they were set)
            _cur->_flags &= ~(VPE::F_INIT | VPE::F_START);
            _cur->notify_resume();

            send_flags(_cur->id(), 0);
            _state = S_IDLE;

            // if we are starting a VPE, we might already have a timeout for it
            if(_ready.length() > 1 && !_timeout) {
                auto callback = std::bind(&ContextSwitcher::start_switch, this, true);
                // timeout immediately, if the VPE is no longer ready
                cycles_t wait = !(_cur->flags() & VPE::F_READY) ? 0 : VPE::TIME_SLICE;
                _timeout = Timeouts::get().wait_for(wait, callback);
            }
            break;
        }
    }

    KLOG(VPES, "CtxSw[" << _pe << "]: done; state=" << stateNames[static_cast<size_t>(_state)]
        << " (current=" << (_cur ? _cur->id() : 0) << ":"
                        << (_cur ? _cur->name().c_str() : "-") << ")");
}

} /* namespace m3 */