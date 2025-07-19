#ifndef TOOLB_SHM_H
#define TOOLB_SHM_H

#include <stdint.h>

// --- Configuration ---
#define REQ_BUFFER_CAPACITY 16
#define RES_BUFFER_CAPACITY 16
#define SHM_NAME "/toolb_ipc"
#define SEM_REQUEST_READY "/toolb_sem_req" //

// --- Data Structures ---
#define METHOD_LEN 8
#define PATH_LEN 256
#define QUERY_PARAMS_LEN 256
#define CONTENT_TYPE_LEN 128 // Increased for multipart boundary
#define BOUNDARY_LEN 70      // New field for multipart boundary
#define AUTH_HEADER_LEN 256
#define BODY_LEN 4096
#define RESPONSE_LEN 4096

// Message from C -> Python
typedef struct {
    uint64_t request_id;
    char method[METHOD_LEN];
    char path[PATH_LEN];
    char query_params[QUERY_PARAMS_LEN];
    char content_type[CONTENT_TYPE_LEN];
    char boundary[BOUNDARY_LEN]; // New field
    char authorization[AUTH_HEADER_LEN];
    int content_length;
    char body[BODY_LEN];
} RequestMessage;

// Message from Python -> C
typedef struct {
    uint64_t request_id;
    int status_code;
    char body[RESPONSE_LEN];
} ResponseMessage;

// Ring buffer structure for requests
typedef struct {
    volatile uint32_t head;
    volatile uint32_t tail;
    RequestMessage requests[REQ_BUFFER_CAPACITY];
} RequestRingBuffer;

// Ring buffer structure for responses
typedef struct {
    volatile uint32_t head;
    volatile uint32_t tail;
    ResponseMessage responses[RES_BUFFER_CAPACITY];
} ResponseRingBuffer;

// The final, top-level shared memory object
typedef struct {
    RequestRingBuffer request_buffer;
    ResponseRingBuffer response_buffer;
} SharedMemoryLayout;

#endif // TOOLB_SHM_H
