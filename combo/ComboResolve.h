#pragma once

// Resolve a combo-ABI symbol from another already-loaded module. `module` is the
// logical name ("soh", "2ship", "comboui"). On Windows each module is a DLL with
// its own symbol namespace, looked up by file name. On ELF platforms the launcher
// dlopens every module RTLD_GLOBAL and the combo ABI is the only surface with
// default visibility (see ComboExport.h), so one process-wide lookup suffices and
// the module name is irrelevant.
#include <cstdio>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

inline void* Combo_ResolveSym(const char* module, const char* name) {
#ifdef _WIN32
    char dll[64];
    snprintf(dll, sizeof(dll), "%s.dll", module);
    HMODULE h = GetModuleHandleA(dll);
    return h != NULL ? reinterpret_cast<void*>(GetProcAddress(h, name)) : nullptr;
#elif defined(__ANDROID__)
    // Android: MainActivity System.loadLibrary()s every module first (so JNI can bind them), and
    // that loads them RTLD_LOCAL — the launcher's later RTLD_GLOBAL dlopen can't be relied on to
    // promote them into the global group. Look the symbol up in the named module instead;
    // RTLD_NOLOAD just returns the handle of the already-loaded library.
    char so[64];
    snprintf(so, sizeof(so), "lib%s.so", module);
    void* h = dlopen(so, RTLD_NOW | RTLD_NOLOAD);
    if (h == nullptr) {
        return nullptr;
    }
    void* sym = dlsym(h, name);
    dlclose(h); // balances the NOLOAD reference; the module itself stays loaded
    return sym;
#else
    (void)module;
    return dlsym(RTLD_DEFAULT, name);
#endif
}
