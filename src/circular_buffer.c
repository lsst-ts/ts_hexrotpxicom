// Modified from the
// "examples/c/circular_buffer/circular_buffer_no_modulo_threadsafe.c" under:
// https://github.com/embeddedartistry/embedded-resources

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <pthread.h>

#include "circular_buffer.h"

static commandStreamStructure_t *buffer;
static pthread_mutex_t lock;

// Checks if the buffer is full
// Requires: cbuf is valid and created by circular_buf_init()
// Returns true if the buffer is full
static bool circular_buf_full(circular_buf_t *cbuf) {
    // We need to handle the wraparound case
    size_t head = cbuf->head + 1;
    if (head == cbuf->max) {
        head = 0;
    }

    return head == cbuf->tail;
}

// Checks if the buffer is empty
// Requires: cbuf is valid and created by circular_buf_init()
// Returns true if the buffer is empty
static bool circular_buf_empty(cbuf_handle_t cbuf) {
    return cbuf->head == cbuf->tail;
}

static void advance_pointer(cbuf_handle_t cbuf) {

    if (circular_buf_full(cbuf)) {
        if (++(cbuf->tail) == cbuf->max) {
            cbuf->tail = 0;
        }
    }

    if (++(cbuf->head) == cbuf->max) {
        cbuf->head = 0;
    }
}

static void retreat_pointer(cbuf_handle_t cbuf) {
    if (++(cbuf->tail) == cbuf->max) {
        cbuf->tail = 0;
    }
}

// Hold the mutex lock.
static inline void hold_lock(void) {
    if (pthread_mutex_lock(&lock) != 0) {
        printf("Mutex lock has failed.\n");
        exit(1);
    }
}

// Release the mutex lock.
static inline void release_lock(void) {
    if (pthread_mutex_unlock(&lock) != 0) {
        printf("Mutex unlock has failed.\n");
        exit(1);
    }
}

// Reset the circular buffer to empty, head == tail. Data not cleared
// Requires: cbuf is valid and created by circular_buf_init()
static void circular_buf_reset(cbuf_handle_t cbuf) {
    cbuf->head = 0;
    cbuf->tail = 0;
}

cbuf_handle_t circular_buf_init(size_t size) {

    // Check the size
    if (size < 1) {
        return NULL;
    }

    // Add one more slot for the head and tail to use
    size_t size_plus_one = size + 1;

    // Allocate the circular buffer
    buffer = (commandStreamStructure_t *)malloc(
        size_plus_one * sizeof(commandStreamStructure_t));
    cbuf_handle_t cbuf = malloc(sizeof(circular_buf_t));

    // Check the memory allocation is successful or not
    if ((buffer == NULL) || (cbuf == NULL)) {
        printf("Memory not allocated.\n");
        return NULL;
    }

    cbuf->buffer = buffer;
    cbuf->max = size_plus_one;
    circular_buf_reset(cbuf);

    if (pthread_mutex_init(&lock, NULL) != 0) {
        printf("Mutex init has failed.\n");
        exit(1);
    }

    return cbuf;
}

void circular_buf_free(cbuf_handle_t cbuf) {
    free(cbuf->buffer);
    free(cbuf);

    if (pthread_mutex_destroy(&lock) != 0) {
        printf("Mutex destroy has failed.\n");
        exit(1);
    }
}

size_t circular_buf_size(cbuf_handle_t cbuf) {
    // Do not consider the wasted slot
    size_t size = cbuf->max - 1;

    hold_lock();

    if (!circular_buf_full(cbuf)) {
        if (cbuf->head >= cbuf->tail) {
            size = (cbuf->head - cbuf->tail);
        } else {
            size = (cbuf->max + cbuf->head - cbuf->tail);
        }
    }

    release_lock();

    return size;
}

size_t circular_buf_capacity(cbuf_handle_t cbuf) {
    // Do not consider the wasted slot
    return cbuf->max - 1;
}

bool circular_buf_put(cbuf_handle_t cbuf, commandStreamStructure_t data) {

    hold_lock();

    // Check there will be the data lost or not
    // This check should be before putting the new data to avoid the
    // mis-judgement that the new added data might make the not-full buffer to
    // to be full
    bool lostData = circular_buf_full(cbuf);

    cbuf->buffer[cbuf->head] = data;
    advance_pointer(cbuf);

    release_lock();

    return lostData;
}

bool circular_buf_get(cbuf_handle_t cbuf, commandStreamStructure_t *data) {

    hold_lock();

    bool isBufEmpty = circular_buf_empty(cbuf);
    if (!isBufEmpty) {
        *data = cbuf->buffer[cbuf->tail];
        retreat_pointer(cbuf);
    }

    release_lock();

    return isBufEmpty;
}
