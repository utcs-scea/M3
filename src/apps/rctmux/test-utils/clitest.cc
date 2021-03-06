/**
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universität Dresden (Germany)
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

#include <base/Common.h>
#include <base/util/Time.h>
#include <base/Panic.h>

#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

#define VERBOSE     0

using namespace m3;

struct App {
    explicit App(int argc, const char *argv[], bool tmux)
        : argc(argc),
          argv(argv),
          vpe(argv[0], VPE::self().pe(), "pager", tmux) {
        if(Errors::last != Errors::NONE)
            exitmsg("Unable to create VPE");
    }

    int argc;
    const char **argv;
    VPE vpe;
};

int main() {
    if(VERBOSE) cout << "Mounting filesystem...\n";
    if(VFS::mount("/", "m3fs") != Errors::NONE)
        PANIC("Cannot mount root fs");

    if(VERBOSE) cout << "Creating VPEs...\n";

    App *apps[3];

    const char *args1[] = {"/bin/rctmux-util-service", "srv1"};
    apps[0] = new App(ARRAY_SIZE(args1), args1, true);

    const char *args2[] = {"/bin/rctmux-util-client", "1", "srv1"};
    apps[1] = new App(ARRAY_SIZE(args2), args2, true);

    const char *args3[] = {"/bin/rctmux-util-client", "2", "srv1"};
    apps[2] = new App(ARRAY_SIZE(args3), args3, true);

    if(VERBOSE) cout << "Starting VPEs...\n";

    for(size_t i = 0; i < ARRAY_SIZE(apps); ++i) {
        apps[i]->vpe.mounts(*VPE::self().mounts());
        apps[i]->vpe.obtain_mounts();
        Errors::Code res = apps[i]->vpe.exec(apps[i]->argc, apps[i]->argv);
        if(res != Errors::NONE)
            PANIC("Cannot execute " << apps[i]->argv[0] << ": " << Errors::to_string(res));
    }

    if(VERBOSE) cout << "Waiting for VPEs...\n";

    // don't wait for the service
    for(size_t i = 1; i < 3; ++i) {
        int res = apps[i]->vpe.wait();
        if(VERBOSE) cout << apps[i]->argv[0] << " exited with " << res << "\n";
    }

    if(VERBOSE) cout << "Deleting VPEs...\n";

    for(size_t i = 0; i < ARRAY_SIZE(apps); ++i)
        delete apps[i];

    if(VERBOSE) cout << "Done\n";
    return 0;
}
