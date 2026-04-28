#include "CrashHandler.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#include <ctime>
#include <cstring>

#pragma comment(lib, "dbghelp.lib")

namespace {

const char* exceptionName(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:        return "ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:   return "ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_DATATYPE_MISALIGNMENT:   return "DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DENORMAL_OPERAND:    return "FLT_DENORMAL_OPERAND";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:      return "FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_INEXACT_RESULT:      return "FLT_INEXACT_RESULT";
        case EXCEPTION_FLT_INVALID_OPERATION:   return "FLT_INVALID_OPERATION";
        case EXCEPTION_FLT_OVERFLOW:            return "FLT_OVERFLOW";
        case EXCEPTION_FLT_STACK_CHECK:         return "FLT_STACK_CHECK";
        case EXCEPTION_FLT_UNDERFLOW:           return "FLT_UNDERFLOW";
        case EXCEPTION_ILLEGAL_INSTRUCTION:     return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR:           return "IN_PAGE_ERROR";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:      return "INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW:            return "INT_OVERFLOW";
        case EXCEPTION_PRIV_INSTRUCTION:        return "PRIV_INSTRUCTION";
        case EXCEPTION_STACK_OVERFLOW:          return "STACK_OVERFLOW";
        default:                                 return "UNKNOWN";
    }
}

LONG WINAPI unhandledFilter(EXCEPTION_POINTERS* info) {
    FILE* f = nullptr;
    fopen_s(&f, "riftwalker_crash.txt", "w");
    if (!f) return EXCEPTION_EXECUTE_HANDLER;

    // Header: timestamp + Windows version stamp via OS
    std::time_t t = std::time(nullptr);
    char ts[64] = {0};
    std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    std::fprintf(f, "Riftwalker crash report\n");
    std::fprintf(f, "Time: %s\n", ts);
    std::fprintf(f, "Code: 0x%08lX (%s)\n",
        info->ExceptionRecord->ExceptionCode,
        exceptionName(info->ExceptionRecord->ExceptionCode));
    std::fprintf(f, "Address: 0x%p\n",
        info->ExceptionRecord->ExceptionAddress);
    std::fprintf(f, "\n--- Stack frames ---\n");

    // Walk the stack
    HANDLE process = GetCurrentProcess();
    HANDLE thread  = GetCurrentThread();
    SymInitialize(process, nullptr, TRUE);
    SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);

    CONTEXT* ctx = info->ContextRecord;
    STACKFRAME64 frame = {};
    DWORD machine;
#ifdef _M_X64
    machine = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset    = ctx->Rip;
    frame.AddrFrame.Offset = ctx->Rbp;
    frame.AddrStack.Offset = ctx->Rsp;
#else
    machine = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset    = ctx->Eip;
    frame.AddrFrame.Offset = ctx->Ebp;
    frame.AddrStack.Offset = ctx->Esp;
#endif
    frame.AddrPC.Mode    = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    char symBuf[sizeof(SYMBOL_INFO) + 256] = {};
    SYMBOL_INFO* sym = reinterpret_cast<SYMBOL_INFO*>(symBuf);
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = 255;

    for (int i = 0; i < 32; ++i) {
        if (!StackWalk64(machine, process, thread, &frame, ctx,
                          nullptr, SymFunctionTableAccess64,
                          SymGetModuleBase64, nullptr)) break;
        if (frame.AddrPC.Offset == 0) break;
        DWORD64 displacement = 0;
        if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, sym)) {
            IMAGEHLP_LINE64 line = {};
            line.SizeOfStruct = sizeof(line);
            DWORD lineDisp = 0;
            if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisp, &line)) {
                std::fprintf(f, "  %2d  %s  (%s:%lu)\n",
                    i, sym->Name, line.FileName, line.LineNumber);
            } else {
                std::fprintf(f, "  %2d  %s  +0x%llX\n",
                    i, sym->Name, (unsigned long long)displacement);
            }
        } else {
            std::fprintf(f, "  %2d  0x%llX\n",
                i, (unsigned long long)frame.AddrPC.Offset);
        }
    }

    SymCleanup(process);
    std::fclose(f);

    // Also pop a message box so the user knows something happened.
    MessageBoxA(nullptr,
        "Riftwalker crashed. A report was written to riftwalker_crash.txt "
        "next to the executable. Please send it to the developer.",
        "Riftwalker — Unexpected Crash", MB_ICONERROR | MB_OK);

    return EXCEPTION_EXECUTE_HANDLER;
}

} // anonymous

namespace CrashHandler {
    void install() {
        SetUnhandledExceptionFilter(unhandledFilter);
    }
}

#endif // _WIN32
