/*
 * Copyright (C) 2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#if defined(__tools__) || defined(__host__)
#   include_next <assert.h>
#else

EXTERN_C void __assert_failed(const char *expr, const char *file, const char *func, int line);

#ifndef NDEBUG

#   define assert(expr)                                                     \
        do {                                                                \
            if(!(expr)) {                                                   \
                __assert_failed(#expr, __FILE__, __FUNCTION__, __LINE__);   \
            }                                                               \
        } while(0)

#else

#   define assert(...)

#endif

#endif
