#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "include/toolb_shm.h"

// --- Globals ---
SharedMemoryLayout* shm_ptr = NULL;
sem_t* request_sem = SEM_FAILED; // Global for the semaphore
int server_fd = -1;
volatile uint64_t request_id_counter = 0;
pthread_mutex_t request_id_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t request_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t response_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

// --- Function Prototypes ---
extern void http_parse_request(RequestMessage* msg, const char* request_str);
void* handle_connection(void* client_socket_ptr);

// Struct to pass arguments to worker threads
typedef struct {
    int client_socket;
    uint64_t request_id;
} thread_args_t;


void cleanup_on_signal(int signum) {
    printf("\n🛡️  [Server] Signal %d received. Shutting down...\n", signum);
    if (shm_ptr != NULL) munmap(shm_ptr, sizeof(SharedMemoryLayout));
    if (request_sem != SEM_FAILED) {
        sem_close(request_sem); // Close the semaphore
    }
    if (server_fd != -1) close(server_fd);
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_REQUEST_READY); // Unlink semaphore from the system
    pthread_mutex_destroy(&request_id_mutex);
    pthread_mutex_destroy(&request_buffer_mutex);
    pthread_mutex_destroy(&response_buffer_mutex);
    printf("✅ [Server] Cleanup complete.\n");
    exit(0);
}

// --- Worker Thread Function ---
void* handle_connection(void* args_ptr) {
    thread_args_t* args = (thread_args_t*)args_ptr;
    int client_socket = args->client_socket;
    uint64_t request_id = args->request_id;
    free(args);

    char buffer[BODY_LEN * 2] = {0};
    read(client_socket, buffer, sizeof(buffer) - 1);

    // --- CRITICAL SECTION for writing to request buffer ---
    pthread_mutex_lock(&request_buffer_mutex);
    uint32_t req_head = shm_ptr->request_buffer.head;
    RequestMessage* msg = &shm_ptr->request_buffer.requests[req_head % REQ_BUFFER_CAPACITY];
    memset(msg, 0, sizeof(RequestMessage));
    msg->request_id = request_id;
    http_parse_request(msg, buffer);
    shm_ptr->request_buffer.head = (req_head + 1) % REQ_BUFFER_CAPACITY;
    pthread_mutex_unlock(&request_buffer_mutex);

    // --- Signal Python that a new request is ready ---
    if (sem_post(request_sem) == -1) {
        perror("FATAL: sem_post failed");
    }

    printf("➡️  [Thread %llu] Request sent and signaled. Path: %s\n", request_id, msg->path);

    // --- Wait for a response from Python ---
    while(1) {
        pthread_mutex_lock(&response_buffer_mutex);
        int found_response = 0;
        if (shm_ptr->response_buffer.tail != shm_ptr->response_buffer.head) {
            ResponseMessage* res = &shm_ptr->response_buffer.responses[shm_ptr->response_buffer.tail % RES_BUFFER_CAPACITY];
            if (res->request_id == request_id) {
                char http_response[RESPONSE_LEN];
                snprintf(http_response, RESPONSE_LEN,
                         "HTTP/1.1 %d OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s",
                         res->status_code, strlen(res->body), res->body);
                send(client_socket, http_response, strlen(http_response), 0);
                shm_ptr->response_buffer.tail = (shm_ptr->response_buffer.tail + 1) % RES_BUFFER_CAPACITY;
                found_response = 1;
            }
        }
        pthread_mutex_unlock(&response_buffer_mutex);

        if (found_response) break;
        usleep(10000);
    }

    close(client_socket);
    printf("THREAD %llu: Connection closed.\n", request_id);
    return NULL;
}

// --- Main (Listener) Thread ---
int main() {
    signal(SIGINT, cleanup_on_signal);

    // 1. Setup Shared Memory
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(SharedMemoryLayout));
    shm_ptr = (SharedMemoryLayout*)mmap(NULL, sizeof(SharedMemoryLayout), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    memset(shm_ptr, 0, sizeof(SharedMemoryLayout));
    printf("✅ [Server] Shared memory initialized.\n");

    // 2. Setup Semaphore
    printf("INFO: Initializing request semaphore...\n");
    sem_unlink(SEM_REQUEST_READY); // Clean up from any previous unclean shutdowns
    request_sem = sem_open(SEM_REQUEST_READY, O_CREAT, 0666, 0); // Initial value of 0
    if (request_sem == SEM_FAILED) {
        perror("FATAL: sem_open failed");
        exit(EXIT_FAILURE);
    }
    printf("✅ [Server] Semaphore initialized.\n");

    // 3. Setup TCP Socket Server
    struct sockaddr_in address;
    int opt = 1;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 30);
    printf("✅ [Server] Listening on http://localhost:8080\n");

    while (1) {
        int client_socket = accept(server_fd, NULL, NULL);
        if (client_socket < 0) continue;

        thread_args_t* args = malloc(sizeof(thread_args_t));
        args->client_socket = client_socket;

        pthread_mutex_lock(&request_id_mutex);
        args->request_id = request_id_counter++;
        pthread_mutex_unlock(&request_id_mutex);

        pthread_t worker_thread;
        pthread_create(&worker_thread, NULL, handle_connection, (void*)args);
        pthread_detach(worker_thread);
    }
    return 0;
}
