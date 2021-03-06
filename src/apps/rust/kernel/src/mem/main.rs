/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

use base::cell::StaticCell;
use base::col::Vec;
use base::errors::{Code, Error};
use base::GlobAddr;
use base::goff;
use core::fmt;
use mem::MemMod;

pub struct MainMemory {
    mods: Vec<MemMod>,
}

pub struct Allocation {
    gaddr: GlobAddr,
    size: usize,
}

impl Allocation {
    pub fn new(gaddr: GlobAddr, size: usize) -> Self {
        Allocation {
            gaddr: gaddr,
            size: size,
        }
    }

    pub fn claim(&mut self) {
        self.size = 0;
    }

    pub fn global(&self) -> GlobAddr {
        self.gaddr
    }
    pub fn size(&self) -> usize {
        self.size
    }
}

impl fmt::Debug for Allocation {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Alloc[addr={:?}, size={:#x}]", self.gaddr, self.size)
    }
}

impl Drop for Allocation {
    fn drop(&mut self) {
        if self.size > 0 {
            get().free(self);
        }
    }
}

impl MainMemory {
    fn new() -> Self {
        MainMemory {
            mods: Vec::new(),
        }
    }

    pub fn add(&mut self, m: MemMod) {
        self.mods.push(m)
    }

    pub fn allocate(&mut self, size: usize, align: usize) -> Result<Allocation, Error> {
        for m in &mut self.mods {
            if let Ok(gaddr) = m.allocate(size, align) {
                klog!(MEM, "Allocated {:#x} bytes at {:?}", size, gaddr);
                return Ok(Allocation::new(gaddr, size))
            }
        }
        Err(Error::new(Code::OutOfMem))
    }
    pub fn allocate_at(&mut self, offset: goff, size: usize) -> Result<Allocation, Error> {
        // TODO check if that's actually ok
        Ok(Allocation::new(self.mods[0].addr() + offset, size))
    }

    pub fn free(&mut self, alloc: &Allocation) {
        for m in &mut self.mods {
            if m.free(alloc.gaddr, alloc.size) {
                klog!(MEM, "Freed {:#x} bytes at {:?}", alloc.size, alloc.gaddr);
                break;
            }
        }
    }

    pub fn capacity(&self) -> usize {
        self.mods.iter().fold(0, |total, ref m| total + m.capacity())
    }
    pub fn available(&self) -> usize {
        self.mods.iter().fold(0, |total, ref m| total + m.available())
    }
}

impl fmt::Debug for MainMemory {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "size: {} MiB, available: {} MiB, mods: [\n",
            self.capacity() / (1024 * 1024), self.available() / (1024 * 1024))?;
        for m in &self.mods {
            write!(f, "  {:?}\n", m)?;
        }
        write!(f, "]")
    }
}

static MEM: StaticCell<Option<MainMemory>> = StaticCell::new(None);

pub fn init() {
    MEM.set(Some(MainMemory::new()));
}

pub fn get() -> &'static mut MainMemory {
    MEM.get_mut().as_mut().unwrap()
}
