#ifndef buffer_h__included
#define buffer_h__included

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "mqtt.h"

typedef struct {
    char *data;         /**< Pointer to data */
    size_t len;         /**< Allocated space in data */
    size_t position;  /**< current cursor position in buffer */
} Buffer;

/**
 * Copy data from current buffer position into dest
 * 
 * This advances the internal buffer position
 * 
 * @param src: Source buffer
 * @param dest: Destination memory area
 * @param len: Number of bytes to copy
 * @returns: Actual number of bytes copied
 */
static inline size_t buffer_copy_out(Buffer *src, char *dest, size_t len) {
    size_t sz = (len > src->len - src->position) ? src->len - src->position : len;
    memcpy(dest, src->data + src->position, sz);
    src->position += sz;
    return sz;
}

/**
 * Copy data into the buffer
 * 
 * This advances the internal buffer position
 * 
 * @param src: Source memory area
 * @param dest: Destination buffer
 * @param len: Number of bytes to copy
 * @returns: Actual number of bytes copied
 */
static inline size_t buffer_copy_in(char *src, Buffer *dest, size_t len) {
    size_t sz = (len > dest->len - dest->position) ? dest->len - dest->position : len;
    memcpy(dest->data + dest->position, src, sz);
    dest->position += sz;
    return sz;
}

/**
 * Get free space in buffer
 * 
 * @param buffer: Buffer to check
 * @returns: Number of free bytes in buffer
 */
static inline size_t buffer_free_space(Buffer *buffer) {
    return buffer->len - buffer->position;
}

/**
 * Check if the internal position is at the end of the buffer
 * 
 * @param buffer; Buffer to check
 * @returns: True if end of buffer reached
 */
static inline bool buffer_eof(Buffer *buffer) {
    return buffer->position == buffer->len;
}

/**
 * Reset internal position of buffer
 * 
 * @param buffer: Buffer to reset
 */
static inline void buffer_reset(Buffer *buffer) {
    buffer->position = 0;
}

/**
 * Allocate a new buffer
 * 
 * @param len: Size of new buffer
 * @returns: New buffer or NULL if out of memory
 */
static inline Buffer *buffer_allocate(size_t len) {
    Buffer *buffer = (Buffer *)malloc(sizeof(Buffer));
    if (buffer == NULL) {
        return NULL;
    }
    buffer->len = len;
    buffer->position = 0;
    buffer->data = (char *)calloc(1, len);
    if (buffer->data == NULL) {
        free(buffer);
        return NULL;
    }

    return buffer;
}

/**
 * Re-allocate buffer size
 * 
 * @param buffer: Buffer to modify
 * @param len: Size of new buffer
 * @returns: Modified buffer if realloc did work, check if buffer->len == len to verify
 */
static inline Buffer *buffer_reallocate(Buffer *buffer, size_t len) {
    char *new_buffer = (char *)realloc(buffer->data, len);
    if (new_buffer != NULL) {
        buffer->data = new_buffer;
        buffer->len = len;
    } 
    return buffer;
}

/**
 * Create a new buffer from a memory area and a size
 * 
 * @param data: Memory area
 * @param len: Length of memory area
 * @returns: New Buffer
 *
 * @attention: the data pointer will be owned by the buffer and freed with it!
 */
static inline Buffer *buffer_from_data_no_copy(char *data, size_t len) {
    Buffer *buffer = (Buffer *)malloc(sizeof(Buffer));
    buffer->len = len;
    buffer->data = data;
    buffer->position = 0;

    return buffer;
}

/**
 * Create a new buffer and copy the data
 * 
 * @param data: Data to copy into the buffer
 * @param len: Number of bytes to copy
 * @returns: New Buffer
 */
static inline Buffer *buffer_from_data_copy(char *data, size_t len) {
    Buffer *buffer = buffer_allocate(len);
    (void)buffer_copy_in(data, buffer, len);
    return buffer;
}

/**
 * Release a buffer
 * 
 * @param buffer: Buffer to release
 */
static inline void buffer_release(Buffer *buffer) {
    free(buffer->data);
    buffer->data = NULL;
    free(buffer);
}

/**
 * Append data to a buffer
 * 
 * @param buffer: Buffer to append data to
 * @param data: Memory area to copy to the end of the buffer
 * @param len: Number of bytes to copy
 * @returns: Numbr of bytes copied
 * 
 * @attention: May come up short if the destination buffer has to be reallocated and
 *             that reallocation fails
 */
static inline size_t buffer_append_data(Buffer *buffer, char *data, size_t len) {
    size_t num_bytes = buffer_copy_in(data, buffer, len);
    if (num_bytes != len) {
        // reallocate
        (void)buffer_reallocate(buffer, buffer->len + (len - num_bytes));
        if (buffer_eof(buffer)) {
            // reallocation failed
            return num_bytes;
        }
        (void)buffer_copy_in(data + num_bytes, buffer, (len - num_bytes));
    }

    return len;
}

/**
 * Append a buffer to another buffer
 * 
 * @param dest: Destination buffer
 * @param src: Source buffer to append
 * @returns: Number of bytes copied
 *
 * @attention: May come up short if the destination buffer has to be reallocated and
 *             that reallocation fails
 */
static inline size_t buffer_append_buffer(Buffer *dest, Buffer *src) {
    return buffer_append_data(dest, src->data, src->len);
}

#if DEBUG
#include "debug.h"

static inline void buffer_hexdump(Buffer *buffer, int indent) {
    hexdump(buffer->data, buffer->len, indent);
}
#else
#define buffer_hexdump(_buffer, _indent) /* */
#endif

#endif /* buffer_h__included */
