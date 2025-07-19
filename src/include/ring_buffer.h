#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h> // For fixed-width integers like uint32_t

// --- Configuration ---
#define BUFFER_CAPACITY 10 // The number of request slots in our buffer

// --- Data Structures ---
#define METHOD_MAX_LEN 8
#define PATH_MAX_LEN 128
#define BODY_MAX_LEN 1024

// The structure for a single request
typedef struct {
    char method[METHOD_MAX_LEN];
    char path[PATH_MAX_LEN];
    int body_len;
    char body[BODY_MAX_LEN];
} RequestMessage;

// The main shared memory layout
// 'volatile' is crucial: it tells the compiler that the value of
// head and tail can be changed by an external process at any time.
typedef struct {
    volatile uint32_t head; // Index of the next free slot to write to
    volatile uint32_t tail; // Index of the next slot to read from
    uint32_t capacity;      // The total capacity of the buffer
    RequestMessage requests[BUFFER_CAPACITY]; // The actual buffer storage
} SharedRingBuffer;

#endif // RING_BUFFER_H