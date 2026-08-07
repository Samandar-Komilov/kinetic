#ifndef KTC_HTTP_RESPONSE_H
#define KTC_HTTP_RESPONSE_H

/**
 * @file response.h
 * @brief HTTP response formatting and construction.
 */

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Formats a compliant HTTP/1.1 empty-body response into a destination character buffer.
 *
 * Automatically generates the protocol line, standard headers (Server, Date, Content-Length: 0,
 * and Connection: close), and the terminating headers-end CRLF pair.
 *
 * @param dest Destination char buffer to write the formatted response string into.
 * @param dest_len The maximum capacity of the destination buffer.
 * @param status The HTTP status code (e.g. 200, 400, 500).
 * @param phrase The HTTP status reason phrase string (e.g. "OK", "Bad Request").
 * @return The number of formatted bytes written (excluding terminating null character), or 0 on
 * overflow.
 */
size_t ktc_response_format_empty(char *dest, size_t dest_len, int status, const char *phrase);

#endif /* KTC_HTTP_RESPONSE_H */
