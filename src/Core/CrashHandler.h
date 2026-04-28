#pragma once
// Windows unhandled-exception filter that writes a crash report
// (timestamp + exception code + stack frames) to riftwalker_crash.txt
// in the working directory. Lets users send a single text file when
// reporting a crash; no minidump tooling required on the user's side.
//
// Linker: requires DbgHelp.lib (Windows SDK; already on every MSVC toolchain).
// Activation: call CrashHandler::install() once at process start, before any
// gameplay code. Filter remains active for the process lifetime.

#ifdef _WIN32
namespace CrashHandler {
    void install();
}
#else
namespace CrashHandler { inline void install() {} }
#endif
