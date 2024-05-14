// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#define _CRT_SECURE_NO_WARNINGS 1

#include <stdbool.h>

#include <uv.h>

#include <dicey/core/errors.h>

#include "trace.h"

#if !defined(NDEBUG)

#if defined(_WIN32)
#define DICEY_TRACE_ENABLED 1

#include <stdio.h>
#include <string.h>

#include <dbghelp.h>
#include <debugapi.h>
#include <winnt.h>

#define is_debugger_present IsDebuggerPresent
#define trigger_breakpoint DebugBreak

static void print_trace(const enum dicey_error errnum) {
    fprintf(stderr, ">>DICEY_TRACE<< error: %s\n", dicey_error_msg(errnum));

    // Get the current thread's stack frame
    CONTEXT context;
    RtlCaptureContext(&context);

    // Initialize the stack walking
    STACKFRAME64 stack_frame;
    memset(&stack_frame, 0, sizeof(STACKFRAME64));

    DWORD machine_type = 0;
#ifdef _M_IX86
    machine_type = IMAGE_FILE_MACHINE_I386;
    stack_frame.AddrPC.Offset = context.Eip;
    stack_frame.AddrFrame.Offset = context.Ebp;
    stack_frame.AddrStack.Offset = context.Esp;
#elif _M_X64
    machine_type = IMAGE_FILE_MACHINE_AMD64;
    stack_frame.AddrPC.Offset = context.Rip;
    stack_frame.AddrFrame.Offset = context.Rbp;
    stack_frame.AddrStack.Offset = context.Rsp;
#elif _M_ARM64
    machine_type = IMAGE_FILE_MACHINE_ARM64;
    stack_frame.AddrPC.Offset = context.Pc;
    stack_frame.AddrFrame.Offset = context.Fp;
    stack_frame.AddrStack.Offset = context.Sp;
#else
#error "Unsupported architecture"
#endif

    stack_frame.AddrPC.Mode = AddrModeFlat;
    stack_frame.AddrFrame.Mode = AddrModeFlat;
    stack_frame.AddrStack.Mode = AddrModeFlat;

    // Walk the stack frames
    while (StackWalk64(
        machine_type,
        GetCurrentProcess(),
        GetCurrentThread(),
        &stack_frame,
        &context,
        NULL,
        SymFunctionTableAccess64,
        SymGetModuleBase64,
        NULL
    )) {
        // Print the stack frame information
        DWORD64 address = stack_frame.AddrPC.Offset;
        char symbol_buffer[sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME * sizeof(TCHAR)];
        PIMAGEHLP_SYMBOL64 symbol = (PIMAGEHLP_SYMBOL64) symbol_buffer;
        symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
        symbol->MaxNameLength = MAX_SYM_NAME;

        DWORD64 displacement;
        if (SymGetSymFromAddr64(GetCurrentProcess(), address, &displacement, symbol)) {
            fprintf(stderr, ">>DICEY_TRACE<< %s\n", symbol->Name);
        } else {
            fprintf(stderr, ">>DICEY_TRACE<< [0x%llX]\n", address);
        }
    }
}

#elif (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))) && !defined(__CYGWIN__)
#define DICEY_TRACE_ENABLED 1

#include <signal.h>
#include <stdio.h>

#include <unistd.h>

#if defined(__linux__)

#include <ctype.h>
#include <string.h>

#include <execinfo.h>

static bool is_debugger_present(void) {
    const int status_fd = open("/proc/self/status", O_RDONLY);
    if (status_fd == -1) {
        return false;
    }

    char buf[4096] = { 0 };
    const ssize_t num_read = read(status_fd, buf, sizeof(buf) - 1);
    close(status_fd);

    if (num_read <= 0) {
        return false;
    }

    buf[num_read] = '\0';

    const char tracerPidString[] = "TracerPid:";
    const char *const tracer_pid_ptr = strstr(buf, tracerPidString);
    if (!tracer_pid_ptr) {
        return false;
    }

    for (const char *characterPtr = tracer_pid_ptr + sizeof(tracerPidString) - 1; characterPtr <= buf + num_read;
         ++characterPtr) {
        if (isspace(*characterPtr)) {
            continue;
        } else {
            return isdigit(*characterPtr) != 0 && *characterPtr != '0';
        }
    }

    return false;
}
#elif defined(__APPLE__) && defined(__MACH__)

#include <assert.h>

#include <sys/sysctl.h>

static bool is_debugger_present(void) {
    struct kinfo_proc info = { 0 };

    // Initialize mib, which tells sysctl the info we want, in this case
    // we're looking for information about a specific process ID.

    int mib[] = {
        [0] = CTL_KERN,
        [1] = KERN_PROC,
        [2] = KERN_PROC_PID,
        [3] = getpid(),
    };

    size_t size = sizeof(info);
    const int junk = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);
    assert(!junk);

    return info.kp_proc.p_flag & P_TRACED;
}

#else // Unsupported OS
static bool is_debugger_present(void) {
    return false;
}
#endif

void trigger_breakpoint(void) {
    raise(SIGTRAP);
}

static void print_trace(const enum dicey_error errnum) {
    void *buffer[32];
    const int nptrs = backtrace(buffer, sizeof buffer / sizeof *buffer);

    fprintf(stderr, ">>DICEY_TRACE<< error: %s\n", dicey_error_msg(errnum));
    backtrace_symbols_fd(buffer + 3, nptrs - 3U, STDERR_FILENO);
}

#else
static inline void print_trace(const enum dicey_error errnum) {
}
#endif

#if defined(DICEY_TRACE_ENABLED)
#include <stdlib.h>
#include <string.h>

#include <uv.h>

static uv_once_t trace_enabled_flag = UV_ONCE_INIT;
static bool trace_enabled = false;
static bool under_debug = false;

static void test_trace_enabled(void) {
    const char *const trace_env = getenv("DICEY_TRACE");

    trace_enabled = trace_env && !strcmp(trace_env, "1");
    under_debug = is_debugger_present();
}

static bool check_trace_enabled(void) {
    uv_once(&trace_enabled_flag, &test_trace_enabled);

    return trace_enabled;
}
#else

#endif

static void trace(const enum dicey_error errnum) {
    if (under_debug) {
        trigger_breakpoint();
    } else {
        print_trace(errnum);
    }
}

enum dicey_error _trace_err(const enum dicey_error errnum) {
    if (errnum && check_trace_enabled()) {
        trace(errnum);
    }

    return errnum;
}

#endif
