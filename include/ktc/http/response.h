#ifndef KTC_HTTP_RESPONSE_H
#define KTC_HTTP_RESPONSE_H

#include <stddef.h>
#include <stdint.h>

/**
 * Formats a compliant HTTP/1.1 empty body response (e.g. 200 OK or errors) into the destination
 * buffer. Automatically adds the Date, Server, Content-Length, and Connection: close headers.
 * Returns the number of formatted bytes, or 0 on overflow.
 */
size_t ktc_response_format_empty(char *dest, size_t dest_len, int status, const char *phrase);

#endif /* KTC_HTTP_RESPONSE_H */
