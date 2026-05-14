#pragma once
#include <cstdlib>
#include <iostream>
#include <execinfo.h>

struct TerminateHook {
    TerminateHook() {
        std::set_terminate([]() {
            void* frames[64];
            int n = backtrace(frames, 64);
            fprintf(stderr, "\n=== std::terminate called ===\n");
            backtrace_symbols_fd(frames, n, STDERR_FILENO);
            fprintf(stderr, "=============================\n");
            _Exit(134);
        });
    }
    static TerminateHook instance;
};
