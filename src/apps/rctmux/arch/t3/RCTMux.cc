#include <m3/arch/t3/RCTMux.h>
#include <m3/DTU.h>
#include <m3/util/Math.h>
#include <m3/util/Sync.h>
#include <m3/util/Profile.h>
#include <string.h>

#include "RCTMux.h"

#define RCTMUX_MAGIC 0x42C0FFEE

using namespace m3;

/**
 * Processor and register state is put into state.cpu_regs by
 * exception-handler.S just before the _interrupt_handler() gets called.
 *
 * Its also used to restore everything when _interrupt_handler()
 * returns.
 */
volatile static struct alignas(DTU_PKG_SIZE) {
    word_t magic;
    word_t *cpu_regs[22];
    uint64_t local_ep_config[EP_COUNT];
    word_t : 8 * sizeof(word_t);    // padding
} _state;

// define an unmangled symbol that can be accessed from assembler
volatile word_t *_regstate = (word_t*)&(_state.cpu_regs);


namespace RCTMux {

void _mem_write(size_t ep, void *data, size_t size, size_t *offset) {
    DTU::get().wait_until_ready(ep);
    DTU::get().write(ep, data, size, *offset);
    *offset += size;
}

void _mem_read(size_t ep, void *data, size_t size, size_t *offset) {
    DTU::get().wait_until_ready(ep);
    DTU::get().read(ep, data, size, *offset);
    *offset += size;
}

inline void _wipe_mem() {
    AppLayout *l = applayout();

    // wipe text to heap
    memset((void*)l->text_start, 0, l->data_size);

    // wipe stack
    memset((void*)_state.cpu_regs[1], 0,
        l->stack_top - (uint32_t)_state.cpu_regs[1]);

    // FIXME: wiping the runtime does make problems - why?
    //memset((void*)RT_SPACE_END, 0, DMEM_VEND - RT_SPACE_END);
}

void setup() {
    _state.magic = RCTMUX_MAGIC;
    flags_reset();
}

void init_switch() {
    // prevent irq from triggering again
    *(volatile unsigned *)IRQ_ADDR_INTERN = 0;

    // save local endpoint config (workaround)
    for(int i = 0; i < EP_COUNT; ++i) {
        _state.local_ep_config[i] = DTU::get().get_ep_config(i);
    }

    flag_set(INITIALIZED);
}

void finish_switch() {
    // restore local endpoint config (workaround)
    for(int i = 0; i < EP_COUNT; ++i) {
        DTU::get().set_ep_config(i, _state.local_ep_config[i]);
    }

    flags_reset();
}

void store() {
    alignas(DTU_PKG_SIZE) uint32_t addr;
    size_t offset = 0;

    // wait for kernel
    while (!flag_is_set(STORAGE_ATTACHED) && !flag_is_set(ERROR))
        ;

    if (flag_is_set(ERROR))
        return;

    // state
    _mem_write(RCTMUX_STORE_EP, (void*)&_state, sizeof(_state), &offset);

    // copy end-area of heap and runtime and keep flags
    addr = Math::round_dn((uintptr_t)(RT_SPACE_END - DTU_PKG_SIZE), DTU_PKG_SIZE);
    _mem_write(RCTMUX_STORE_EP, (void*)addr, DMEM_VEND - addr, &offset);

    // app layout
    AppLayout *l = applayout();
    _mem_write(RCTMUX_STORE_EP, (void*)l, sizeof(*l), &offset);

    // reset vector
    _mem_write(RCTMUX_STORE_EP, (void*)l->reset_start, l->reset_size, &offset);

    // text
    _mem_write(RCTMUX_STORE_EP, (void*)l->text_start, l->text_size, &offset);

    // data and heap
    _mem_write(RCTMUX_STORE_EP, (void*)l->data_start, l->data_size, &offset);

    // copy stack
    addr = (uint32_t)_state.cpu_regs[1] - REGSPILL_AREA_SIZE;
    _mem_write(RCTMUX_STORE_EP, (void*)addr,
        Math::round_dn((uintptr_t)l->stack_top - addr, DTU_PKG_SIZE),
        &offset);

    _wipe_mem();

    // success
    flag_unset(STORE);
}

void restore() {
    alignas(DTU_PKG_SIZE) uint32_t addr;
    size_t offset = 0;

    while (!flag_is_set(STORAGE_ATTACHED) && !flag_is_set(ERROR))
        ;

    if (flag_is_set(ERROR))
        return;

    // read state
    _mem_read(RCTMUX_RESTORE_EP, (void*)&_state, sizeof(_state), &offset);

    if (_state.magic != RCTMUX_MAGIC) {
        flag_set(ERROR);
        return;
    }

    // restore end-area of heap and runtime before accessing applayout
    addr = Math::round_dn((uintptr_t)(RT_SPACE_END - DTU_PKG_SIZE), DTU_PKG_SIZE);
    _mem_read(RCTMUX_RESTORE_EP, (void*)addr, DMEM_VEND - addr, &offset);

    // restore app layout
    AppLayout *l = applayout();
    _mem_read(RCTMUX_RESTORE_EP, (void*)l, sizeof(*l), &offset);

    // restore reset vector
    _mem_read(RCTMUX_RESTORE_EP, (void*)l->reset_start, l->reset_size, &offset);

    // restore text
    _mem_read(RCTMUX_RESTORE_EP, (void*)l->text_start, l->text_size, &offset);

    // restore data and heap
    _mem_read(RCTMUX_RESTORE_EP, (void*)l->data_start, l->data_size, &offset);

    // restore stack
    addr = ((uint32_t)_state.cpu_regs[1]) - REGSPILL_AREA_SIZE;
    _mem_read(RCTMUX_RESTORE_EP, (void*)addr,
        Math::round_up((uintptr_t)l->stack_top - addr, DTU_PKG_SIZE),
        &offset);

    // success
    flag_unset(RESTORE);
}

void reset() {
    // simulate reset since resetting the PE from kernel side is not
    // currently supported for t3
    // TODO
}

void set_idle_mode() {
    // set epc (exception program counter) to jump into idle mode
    // when returning from exception
    _state.cpu_regs[EPC_REG] = (word_t*)&_start;
}

} /* namespace RCTMux */