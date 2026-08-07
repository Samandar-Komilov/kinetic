#ifndef KTC_CONNECTION_H
#define KTC_CONNECTION_H

#include <stdbool.h>
#include <uv.h>

#define KTC_MAX_CONNECTIONS 1024
#define KTC_GLOBAL_POOL_SLOT_SIZE 8096 // 8KB

/**
 * @brief Allocates the global buffer pool for connection management.
 */
void ktc_connection_pool_init(void);

/**
 * @brief Deallocates the global buffer pool.
 */
void ktc_connection_pool_destroy(void);

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