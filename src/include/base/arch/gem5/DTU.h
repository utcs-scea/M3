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

#include <base/Common.h>
#include <base/Env.h>
#include <base/util/Util.h>
#include <base/util/Sync.h>
#include <base/Errors.h>
#include <assert.h>

#define DTU_PKG_SIZE        (static_cast<size_t>(8))

namespace kernel {
class DTU;
class DTURegs;
class DTUState;
class VPE;
}

namespace m3 {

class DTU {
    friend class kernel::DTU;
    friend class kernel::DTURegs;
    friend class kernel::DTUState;
    friend class kernel::VPE;

    explicit DTU() {
    }

public:
    typedef uint64_t reg_t;

private:
    static const uintptr_t BASE_ADDR        = 0xF0000000;
    static const size_t DTU_REGS            = 10;
    static const size_t CMD_REGS            = 7;
    static const size_t EP_REGS             = 3;

    static const size_t CREDITS_UNLIM       = 0xFFFF;
    static const size_t MAX_PKT_SIZE        = 1024;

    enum class DtuRegs {
        FEATURES            = 0,
        ROOT_PT             = 1,
        PF_EP               = 2,
        LAST_PF             = 3,
        RW_BARRIER          = 4,
        VPE_ID              = 5,
        CUR_TIME            = 6,
        IDLE_TIME           = 7,
        MSG_CNT             = 8,
        EXT_CMD             = 9,
    };

    enum class CmdRegs {
        COMMAND             = 10,
        ABORT               = 11,
        DATA_ADDR           = 12,
        DATA_SIZE           = 13,
        OFFSET              = 14,
        REPLY_EP            = 15,
        REPLY_LABEL         = 16,
    };

    enum MemFlags : reg_t {
        R                   = 1 << 0,
        W                   = 1 << 1,
    };

    enum StatusFlags : reg_t {
        PRIV                = 1 << 0,
        PAGEFAULTS          = 1 << 1,
    };

    enum class EpType {
        INVALID,
        SEND,
        RECEIVE,
        MEMORY
    };

    enum class CmdOpCode {
        IDLE                = 0,
        SEND                = 1,
        REPLY               = 2,
        READ                = 3,
        WRITE               = 4,
        FETCH_MSG           = 5,
        ACK_MSG             = 6,
        SLEEP               = 7,
        DEBUG_MSG           = 8,
    };

    enum class ExtCmdOpCode {
        IDLE                = 0,
        WAKEUP_CORE         = 1,
        INV_PAGE            = 2,
        INV_TLB             = 3,
        INJECT_IRQ          = 4,
        RESET               = 5,
    };

public:
    typedef uint64_t pte_t;

    enum CmdFlags {
        NOPF                = 1,
    };

    enum {
        PTE_BITS            = 3,
        PTE_SIZE            = 1 << PTE_BITS,
        LEVEL_CNT           = 2,
        LEVEL_BITS          = PAGE_BITS - PTE_BITS,
        LEVEL_MASK          = (1 << LEVEL_BITS) - 1,
        PTE_REC_IDX         = LEVEL_MASK,
    };

    enum {
        PTE_R               = 1,
        PTE_W               = 2,
        PTE_X               = 4,
        PTE_I               = 8,
        PTE_GONE            = 16,
        PTE_RW              = PTE_R | PTE_W,
        PTE_RWX             = PTE_RW | PTE_X,
        PTE_IRWX            = PTE_RWX | PTE_I,
    };

    enum {
        ABORT_VPE           = 1,
        ABORT_CMD           = 2,
    };

    struct Header {
        uint8_t flags; // if bit 0 is set its a reply, if bit 1 is set we grant credits
        uint8_t senderCoreId;
        uint8_t senderEpId;
        uint8_t replyEpId; // for a normal message this is the reply epId
                           // for a reply this is the enpoint that receives credits
        uint16_t length;
        uint16_t senderVpeId;

        uint64_t label;
        uint64_t replylabel;
    } PACKED;

    struct Message : Header {
        int send_epid() const {
            return senderEpId;
        }
        int reply_epid() const {
            return replyEpId;
        }

        unsigned char data[];
    } PACKED;

    static const size_t HEADER_SIZE         = sizeof(Header);

    // TODO not yet supported
    static const int FLAG_NO_RINGBUF        = 0;
    static const int FLAG_NO_HEADER         = 1;

    static const int MEM_EP                 = 0;    // unused
    static const int SYSC_EP                = 0;
    static const int DEF_RECVEP             = 1;
    static const int FIRST_FREE_EP          = 2;

    static DTU &get() {
        return inst;
    }

    static size_t noc_to_pe(uint64_t noc) {
        return (noc >> 52) - 0x80;
    }
    static uintptr_t noc_to_virt(uint64_t noc) {
        return noc & ((static_cast<uint64_t>(1) << 52) - 1);
    }
    static uint64_t build_noc_addr(int pe, uintptr_t virt) {
        return (static_cast<uintptr_t>(0x80 + pe) << 52) | virt;
    }

    uintptr_t get_last_pf() const {
        return read_reg(DtuRegs::LAST_PF);
    }

    Errors::Code send(int ep, const void *msg, size_t size, label_t replylbl, int reply_ep);
    Errors::Code reply(int ep, const void *msg, size_t size, size_t off);
    Errors::Code read(int ep, void *msg, size_t size, size_t off, uint flags);
    Errors::Code write(int ep, const void *msg, size_t size, size_t off, uint flags);
    Errors::Code cmpxchg(int, const void *, size_t, size_t, size_t) {
        // TODO unsupported
        return Errors::NO_ERROR;
    }

    void abort(uint flags, reg_t *cmdreg) {
        *cmdreg = read_reg(CmdRegs::COMMAND);
        write_reg(CmdRegs::ABORT, flags);
        if(get_error() != Errors::ABORT)
            *cmdreg = static_cast<reg_t>(CmdOpCode::IDLE);
    }
    void retry(reg_t cmd) {
        write_reg(CmdRegs::COMMAND, cmd);
    }

    bool is_valid(int epid) const {
        reg_t r0 = read_reg(epid, 0);
        return static_cast<EpType>(r0 >> 61) != EpType::INVALID;
    }

    Message *fetch_msg(int epid) const {
        write_reg(CmdRegs::COMMAND, buildCommand(epid, CmdOpCode::FETCH_MSG));
        Sync::memory_barrier();
        return reinterpret_cast<Message*>(read_reg(CmdRegs::OFFSET));
    }

    size_t get_msgoff(int, const Message *msg) const {
        return reinterpret_cast<uintptr_t>(msg);
    }

    void mark_read(int ep, size_t off) {
        write_reg(CmdRegs::OFFSET, off);
        // ensure that we are really done with the message before acking it
        Sync::memory_barrier();
        write_reg(CmdRegs::COMMAND, buildCommand(ep, CmdOpCode::ACK_MSG));
        // ensure that we don't do something else before the ack
        Sync::memory_barrier();
    }

    uint msgcnt() {
        return read_reg(DtuRegs::MSG_CNT);
    }
    void try_sleep(bool report = true, uint64_t cycles = 0);
    void sleep(uint64_t cycles = 0) {
        write_reg(CmdRegs::OFFSET, cycles);
        Sync::memory_barrier();
        write_reg(CmdRegs::COMMAND, buildCommand(0, CmdOpCode::SLEEP));
    }

    void wait_until_ready(int) const {
        // this is superfluous now, but leaving it here improves the syscall time by 40 cycles (!!!)
        // compilers are the worst. let's get rid of them and just write assembly code again ;)
        while((read_reg(CmdRegs::COMMAND) & 0x7) != 0)
            ;
    }
    bool wait_for_mem_cmd() const {
        // we've already waited
        return true;
    }

    void debug_msg(uint msg) {
        write_reg(CmdRegs::OFFSET, msg);
        Sync::memory_barrier();
        write_reg(CmdRegs::COMMAND, buildCommand(0, CmdOpCode::DEBUG_MSG));
    }

private:
    Errors::Code transfer(reg_t cmd, uintptr_t data, size_t size, size_t off);

    static Errors::Code get_error() {
        while(true) {
            reg_t cmd = read_reg(CmdRegs::COMMAND);
            if(static_cast<CmdOpCode>(cmd & 0x7) == CmdOpCode::IDLE)
                return static_cast<Errors::Code>(cmd >> 12);
        }
        UNREACHED;
    }

    static reg_t read_reg(DtuRegs reg) {
        return read_reg(static_cast<size_t>(reg));
    }
    static reg_t read_reg(CmdRegs reg) {
        return read_reg(static_cast<size_t>(reg));
    }
    static reg_t read_reg(int ep, size_t idx) {
        return read_reg(DTU_REGS + CMD_REGS + EP_REGS * ep + idx);
    }
    static reg_t read_reg(size_t idx) {
        reg_t res;
        asm volatile (
            "mov (%1), %0"
            : "=r"(res)
            : "r"(BASE_ADDR + idx * sizeof(reg_t))
        );
        return res;
    }

    static void write_reg(DtuRegs reg, reg_t value) {
        write_reg(static_cast<size_t>(reg), value);
    }
    static void write_reg(CmdRegs reg, reg_t value) {
        write_reg(static_cast<size_t>(reg), value);
    }
    static void write_reg(size_t idx, reg_t value) {
        asm volatile (
            "mov %0, (%1)"
            : : "r"(value), "r"(BASE_ADDR + idx * sizeof(reg_t))
        );
    }

    static uintptr_t dtu_reg_addr(DtuRegs reg) {
        return BASE_ADDR + static_cast<size_t>(reg) * sizeof(reg_t);
    }
    static uintptr_t cmd_reg_addr(CmdRegs reg) {
        return BASE_ADDR + static_cast<size_t>(reg) * sizeof(reg_t);
    }
    static uintptr_t ep_regs_addr(int ep) {
        return BASE_ADDR + (DTU_REGS + CMD_REGS + ep * EP_REGS) * sizeof(reg_t);
    }

    static reg_t buildCommand(int epid, CmdOpCode c, uint flags = 0) {
        return static_cast<reg_t>(c) |
                (static_cast<reg_t>(epid) << 3) |
                (static_cast<reg_t>(flags) << 11);
    }

    static DTU inst;
};

}