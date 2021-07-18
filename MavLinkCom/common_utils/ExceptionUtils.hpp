// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef air_ExceptionsUtils_hpp
#define air_ExceptionsUtils_hpp

//print exception stack trace
//from: http://stackoverflow.com/a/11674810/207661

#ifdef __GNUC__

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-pedantic"

#include <dlfcn.h>
#include <execinfo.h>
#include <typeinfo>
#include <string>
#include <memory>
#include <cxxabi.h>
#include <cstdlib>

namespace
{
void* last_frames[20];
size_t last_size;
std::string exception_name;

std::string demangle(const char* name)
{
    int status;
    std::unique_ptr<char, void (*)(void*)> realname(abi::__cxa_demangle(name, 0, 0, &status), &std::free);
    return status ? "failed" : &*realname;
}
}

extern "C" {
void __cxa_throw(void* ex, void* info, void (*dest)(void*))
{
    exception_name = demangle(reinterpret_cast<const std::type_info*>(info)->name());
    last_size = backtrace(last_frames, sizeof last_frames / sizeof(void*));

    static void (*const rethrow)(void*, void*, void (*)(void*)) __attribute__((noreturn)) = (void (*)(void*, void*, void (*)(void*)))dlsym(RTLD_NEXT, "__cxa_throw");
    rethrow(ex, info, dest);
}
}

#pragma GCC diagnostic pop
#endif

void printExceptionStack()
{
#ifdef __GNUC__
    backtrace_symbols_fd(last_frames, last_size, 2);
#endif
}

#endif
