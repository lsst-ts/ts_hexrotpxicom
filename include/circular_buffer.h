#ifndef CIRCULARBUFFER_H
#define CIRCULARBUFFER_H

// Modified from the "examples/c/circular_buffer/circular_buffer.h" under:
// https://github.com/embeddedartistry/embedded-resources

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "commandStructure.h"

// The definition of our circular buffer structure
struct circular_buf_t {
    commandStreamStructure_t *buffer;
    size_t head;
    size_t tail;
    size_t max; // of the buffer
};

// Circular buffer structure
typedef struct circular_buf_t circular_buf_t;

// Handle type, the way users interact with the API
typedef circular_buf_t *cbuf_handle_t;

// Pass in a buffer size, returns a circular buffer handle
// Requires: buffer size > 0
// Ensures: cbuf has been created and is returned in an empty state
// Return NULL if the size < 1 or memory allocation fails
cbuf_handle_t circular_buf_init(size_t size);

// Free a circular buffer structure
// Requires: cbuf is valid and created by circular_buf_init()
void circular_buf_free(cbuf_handle_t cbuf);

// Add data to the buffer. Delete old data to make room, if necessary.
// Return true if old data was deleted, else false.
// Requires: cbuf is valid and created by circular_buf_init()
// This is a thread-safe function
bool circular_buf_put(cbuf_handle_t cbuf, commandStreamStructure_t data);

// Retrieve the data from the buffer.
// Pop and return the oldest data from the buffer.
// Return true if the buffer was empty (no data returned), false otherwise.
// Requires: cbuf is valid and created by circular_buf_init()
// This is a thread-safe function
bool circular_buf_get(cbuf_handle_t cbuf, commandStreamStructure_t *data);

// Get the capacity of the buffer
// Requires: cbuf is valid and created by circular_buf_init()
// Returns the maximum capacity of the buffer
size_t circular_buf_capacity(cbuf_handle_t cbuf);

// Get the number of elements stored in the buffer
// Requires: cbuf is valid and created by circular_buf_init()
// Returns the current number of elements in the buffer
// This is a thread-safe function
size_t circular_buf_size(cbuf_handle_t cbuf);

#endif // CIRCULARBUFFER_H
