#pragma once
// Applied only when compiling the dependency, so C diagnostics stay off stdout.
#include <cstdarg>
#include <cstdio>
inline int foggy_transport_log(FILE *stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vfprintf(stream == stdout ? stderr : stream, format, args);
    va_end(args);
    return result;
}
#define printf(...) foggy_transport_log(stderr, __VA_ARGS__)
#define fprintf(stream, ...) foggy_transport_log(stream, __VA_ARGS__)
