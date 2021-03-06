/*
 * Copyright (C) 2016, René Küttner <rene.kuettner@tu-dresden.de>
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

#if defined(__t3__)
#   include <base/arch/t3/RCTMux.h>
#endif

namespace m3 {

/**
 * These flags implement the flags register for remote controlled time-multiplexing, which is used
 * to synchronize rctmux and the kernel. The kernel sets the flags register to let rctmux know
 * about the required operation. Rctmux signals completion to the kernel afterwards.
 */
enum RCTMuxCtrl {
    NONE                = 0,
    STORE               = 1 << 0, // store operation required
    RESTORE             = 1 << 1, // restore operation required
    WAITING             = 1 << 2, // set by the kernel if a signal is required
    SIGNAL              = 1 << 3, // used to signal completion to the kernel
};

}
