#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "include/toolb_shm.h"
#include "include/config.h" // For config file handling

// --- Globals ---
SharedMemoryLayout* shm_ptr = NULL;
sem_t* request_sem = SEM_FAILED;
int server_fd = -1;
SSL_CTX* ssl_ctx = NULL;
volatile uint64_t request_id_counter = 0;
pthread_mutex_t request_id_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t request_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t response_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

// --- Externs from config.c ---
extern AppConfig config;
extern void load_config(const char* filename);

// --- Function Prototypes ---
extern void http_parse_request(RequestMessage* msg, const char* request_str);
void* handle_connection(void* client_socket_ptr);

// Struct to pass arguments to worker threads
typedef struct {
    int client_socket;
    uint64_t request_id;
    SSL* ssl;
} thread_args_t;

// --- Structured Logging Function ---
void log_message(const char* level, uint64_t thread_id, const char* message) {
    time_t now = time(NULL);
    char time_buf[sizeof("2025-07-21T13:43:00Z")];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    printf("{\"timestamp\":\"%s\", \"level\":\"%s\", \"source\":\"c_server\", \"thread_id\":%llu, \"message\":\"%s\"}\n",
           time_buf, level, thread_id, message);
    fflush(stdout);
}

// --- Auto-Certificate Generation ---
void check_and_generate_certs() {
    if (access(config.cert_file, F_OK) == -1 || access(config.key_file, F_OK) == -1) {
        log_message("INFO", 0, "SSL certificate not found. Generating self-signed certificate...");
        char command[512];
        snprintf(command, sizeof(command),
                 "openssl req -x509 -newkey rsa:4096 -nodes "
                 "-keyout %s -out %s "
                 "-sha256 -days 365 "
                 "-subj \"/C=US/ST=CA/L=SF/O=toolB/CN=localhost\" > /dev/null 2>&1",
                 config.key_file, config.cert_file);
        if (system(command) != 0) {
            log_message("FATAL", 0, "Failed to generate SSL certificate. Please ensure OpenSSL is installed.");
            exit(EXIT_FAILURE);
        }
        log_message("INFO", 0, "Certificate generated successfully.");
    }
}

void cleanup_on_signal(int signum) {
    printf("\n");
    log_message("INFO", 0, "Signal received. Shutting down...");
    if (shm_ptr != NULL) munmap(shm_ptr, sizeof(SharedMemoryLayout));
    if (request_sem != SEM_FAILED) sem_close(request_sem);
    if (server_fd != -1) close(server_fd);
    if (ssl_ctx != NULL) SSL_CTX_free(ssl_ctx);
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_REQUEST_READY);
    pthread_mutex_destroy(&request_id_mutex);
    pthread_mutex_destroy(&request_buffer_mutex);
    pthread_mutex_destroy(&response_buffer_mutex);
    log_message("INFO", 0, "Cleanup complete.");
    exit(0);
}

void init_openssl() { SSL_load_error_strings(); OpenSSL_add_ssl_algorithms(); }

SSL_CTX* create_ssl_context() {
    const SSL_METHOD* method = TLS_server_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) { log_message("FATAL", 0, "Unable to create SSL context"); ERR_print_errors_fp(stderr); exit(EXIT_FAILURE); }
    return ctx;
}

void configure_ssl_context(SSL_CTX* ctx) {
    if (SSL_CTX_use_certificate_file(ctx, config.cert_file, SSL_FILETYPE_PEM) <= 0) {
        log_message("FATAL", 0, "Failed to load certificate file from config."); ERR_print_errors_fp(stderr); exit(EXIT_FAILURE);
    }
    if (SSL_CTX_use_PrivateKey_file(ctx, config.key_file, SSL_FILETYPE_PEM) <= 0) {
        log_message("FATAL", 0, "Failed to load private key file from config."); ERR_print_errors_fp(stderr); exit(EXIT_FAILURE);
    }
}

void* handle_connection(void* args_ptr) {
    thread_args_t* args = (thread_args_t*)args_ptr;
    int client_socket = args->client_socket;
    uint64_t request_id = args->request_id;
    SSL* ssl = args->ssl;
    free(args);

    if (SSL_accept(ssl) <= 0) {
        log_message("ERROR", request_id, "SSL handshake failed.");
        ERR_print_errors_fp(stderr);
    } else {
        char buffer[BODY_LEN * 2] = {0};
        SSL_read(ssl, buffer, sizeof(buffer) - 1);

        pthread_mutex_lock(&request_buffer_mutex);
        uint32_t req_head = shm_ptr->request_buffer.head;
        RequestMessage* msg = &shm_ptr->request_buffer.requests[req_head % REQ_BUFFER_CAPACITY];
        memset(msg, 0, sizeof(RequestMessage));
        msg->request_id = request_id;
        http_parse_request(msg, buffer);
        shm_ptr->request_buffer.head = (req_head + 1) % REQ_BUFFER_CAPACITY;
        pthread_mutex_unlock(&request_buffer_mutex);

        if (sem_post(request_sem) == -1) { log_message("FATAL", request_id, "sem_post failed"); }
        log_message("INFO", request_id, "Request sent to Python app.");

        time_t start_time = time(NULL);
        int found_response = 0;
        while(time(NULL) - start_time < config.timeout_seconds) {
            pthread_mutex_lock(&response_buffer_mutex);
            if (shm_ptr->response_buffer.tail != shm_ptr->response_buffer.head) {
                ResponseMessage* res = &shm_ptr->response_buffer.responses[shm_ptr->response_buffer.tail % RES_BUFFER_CAPACITY];
                if (res->request_id == request_id) {
                    char http_response[RESPONSE_LEN];
                    snprintf(http_response, RESPONSE_LEN,
                             "HTTP/1.1 %d OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s",
                             res->status_code, strlen(res->body), res->body);
                    SSL_write(ssl, http_response, strlen(http_response));
                    shm_ptr->response_buffer.tail = (shm_ptr->response_buffer.tail + 1) % RES_BUFFER_CAPACITY;
                    found_response = 1;
                }
            }
            pthread_mutex_unlock(&response_buffer_mutex);
            if (found_response) break;
            usleep(10000);
        }

        if (!found_response) {
            log_message("WARN", request_id, "Timed out waiting for response from Python.");
            const char* timeout_response = "HTTP/1.1 504 Gateway Timeout\r\nContent-Length: 0\r\n\r\n";
            SSL_write(ssl, timeout_response, strlen(timeout_response));
        }
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_socket);
    log_message("INFO", request_id, "Connection closed.");
    return NULL;
}

int main() {
    signal(SIGINT, cleanup_on_signal);

    load_config("toolb.conf");
    char log_buf[256];
    sprintf(log_buf, "Initializing toolB server with config from toolb.conf");
    log_message("INFO", 0, log_buf);

    check_and_generate_certs();
    init_openssl();
    ssl_ctx = create_ssl_context();
    configure_ssl_context(ssl_ctx);

    sem_unlink(SEM_REQUEST_READY);
    request_sem = sem_open(SEM_REQUEST_READY, O_CREAT, 0666, 0);
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(SharedMemoryLayout));
    shm_ptr = (SharedMemoryLayout*)mmap(NULL, sizeof(SharedMemoryLayout), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    memset(shm_ptr, 0, sizeof(SharedMemoryLayout));
    log_message("INFO", 0, "Shared memory and semaphore initialized.");

    struct sockaddr_in address;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(config.port);
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 30);

    sprintf(log_buf, "Server listening on https://localhost:%d", config.port);
    log_message("INFO", 0, log_buf);

    while (1) {
        log_message("DEBUG", 0, "Waiting for new connection...");
        int client_socket = accept(server_fd, NULL, NULL);
        if (client_socket < 0) continue;

        SSL* ssl = SSL_new(ssl_ctx);
        SSL_set_fd(ssl, client_socket);

        thread_args_t* args = malloc(sizeof(thread_args_t));
        args->client_socket = client_socket;
        args->ssl = ssl;

        pthread_mutex_lock(&request_id_mutex);
        args->request_id = request_id_counter++;
        pthread_mutex_unlock(&request_id_mutex);

        pthread_t worker_thread;
        pthread_create(&worker_thread, NULL, handle_connection, (void*)args);
        pthread_detach(worker_thread);
    }
    return 0;
}
