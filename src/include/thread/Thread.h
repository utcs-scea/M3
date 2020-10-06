/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Copyright (C) 2016, Matthias Hille <matthias.hille@tu-dresden.de>
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
#include <base/util/String.h>

#if defined(__x86_64__)
#   include <thread/isa/x86_64/Thread.h>
#elif defined(__arm__)
#   include <thread/isa/arm/Thread.h>
#elif defined(__aarch64__)
#   include <thread/isa/aarch64/Thread.h>
#else
#   error "Unsupported ISA"
#endif

namespace m3 {

class ThreadManager;

class Thread : public SListItem {
    friend class ThreadManager;

    static constexpr size_t T_STACK_WORDS   = m3::T_STACK_WORDS;
    static constexpr size_t T_STACK_SZ      = T_STACK_WORDS * sizeof(word_t);
    static constexpr size_t MAX_MSG_SIZE    = 1024;

public:
    typedef _thread_func thread_func;

    explicit Thread(thread_func func, void *arg);
    ~Thread();

private:
    explicit Thread()
        : _id(_next_id++),
          _regs(),
          _stack(),
          _event(0),
          _content(false) {
    }

    bool save() {
        return thread_save(&_regs);
    }
    bool resume() {
        return thread_resume(&_regs);
    }

    void subscribe(event_t event) {
        _event = event;
    }
    void unsubscribe(event_t event) {
        if(_event == event)
            _event = 0;
    }
    void set_msg(const void *msg, size_t size) {
        _content = msg != nullptr;
        if(msg)
            memcpy(_msg, msg, (size > MAX_MSG_SIZE) ? MAX_MSG_SIZE : size);
    }

public:
    int id() const {
        return _id;
    }
    const Regs &regs() const {
        return _regs;
    }
    inline bool trigger_event(event_t event) const {
        return _event == event;
    }
    const unsigned char *get_msg() const {
        return _content ? _msg : nullptr;
    }

private:
    int _id;
    Regs _regs;
    word_t *_stack;
    event_t _event;
    bool _content;
    unsigned char _msg[MAX_MSG_SIZE];
    static int _next_id;
};

}
