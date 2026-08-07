#include "ktc/http/response.h"

#include <stdio.h>
#include <time.h>

size_t ktc_response_format_empty(char *dest, size_t dest_len, int status, const char *phrase) {
    time_t rawtime;
    time(&rawtime);
    struct tm result;
    // Use gmtime_r as gmtime is not reentrant (thread-safe) (Bug 10)
    struct tm *info = gmtime_r(&rawtime, &result);
    if (!info) {
        return 0;
    }

    char date_str[64];
    size_t rc = strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", info);
    if (rc == 0) {
        return 0;
    }

    int written = snprintf(dest, dest_len,
                           "HTTP/1.1 %d %s\r\n"
                           "Date: %s\r\n"
                           "Server: kinetic\r\n"
                           "Content-Length: 0\r\n"
                           "Connection: close\r\n"
                           "\r\n",
                           status, phrase, date_str);

    if (written < 0 || (size_t)written >= dest_len) {
        return 0; // Buffer overflow occurred or snprintf failed
    }

    return (size_t)written;
}
