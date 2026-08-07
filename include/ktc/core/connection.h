#ifndef KTC_CONNECTION_H
#define KTC_CONNECTION_H

#include <stdbool.h>
#include <uv.h>

/**
 * @brief Callback invoked by libuv when a new incoming client connection is ready to be accepted.
 *
 * This function handles the connection lifecycle initiation. It allocates connection-specific
 * resources (such as the arena and buffers), accepts the client socket, sets TCP_NODELAY,
 * and starts reading request data from the client asynchronously.
 *
 * @param server The server stream handle listening for connections.
 * @param status 0 if a connection is ready, or a negative libuv error code on failure.
 */
void ktc_on_connection(uv_stream_t *server, int status);

/**
 * @brief Sets the global shutdown flag for the connection subsystem.
 *
 * When set to true, any subsequent incoming connections received via @ref ktc_on_connection
 * will be ignored/dropped immediately without allocating resources or accepting them.
 *
 * @param is_shutting_down true to reject new connections, false to accept them.
 */
void ktc_connections_set_shutting_down(bool is_shutting_down);

#endif // KTC_CONNECTION_H