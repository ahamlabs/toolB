#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "include/toolb_shm.h"

// Helper to find a substring and return a pointer to the character AFTER the needle.
static const char* find_after(const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    const char* found = strstr(haystack, needle);
    if (found) {
        return found + strlen(needle);
    }
    return NULL;
}

// Helper to copy a value between a start and end pointer safely.
static void copy_value(char* dest, const char* start, const char* end, int max_len) {
    if (!dest || !start || !end || start >= end) {
        if (dest) dest[0] = '\0';
        return;
    }
    int len = end - start;
    if (len >= max_len) {
        len = max_len - 1;
    }
    strncpy(dest, start, len);
    dest[len] = '\0';
}

// The C implementation of the HTTP parser.
void http_parse_request(RequestMessage* msg, const char* request_str) {
    if (!msg || !request_str) return;
    memset(msg->boundary, 0, BOUNDARY_LEN); // Ensure boundary is clear initially

    // 1. Parse Method
    const char* method_end = strchr(request_str, ' ');
    if (!method_end) return;
    copy_value(msg->method, request_str, method_end, METHOD_LEN);

    // 2. Parse Path and Query Parameters
    const char* path_start = method_end + 1;
    const char* path_end = strchr(path_start, ' ');
    if (!path_end) return;

    const char* query_start = strchr(path_start, '?');
    if (query_start && query_start < path_end) {
        copy_value(msg->path, path_start, query_start, PATH_LEN);
        copy_value(msg->query_params, query_start + 1, path_end, QUERY_PARAMS_LEN);
    } else {
        copy_value(msg->path, path_start, path_end, PATH_LEN);
        msg->query_params[0] = '\0';
    }

    // 3. Find and Parse Headers
    const char* content_type_start = find_after(request_str, "Content-Type: ");
    if (content_type_start) {
        const char* content_type_end = strstr(content_type_start, "\r\n");
        copy_value(msg->content_type, content_type_start, content_type_end, CONTENT_TYPE_LEN);

        // Check for multipart and extract boundary
        const char* boundary_start = find_after(msg->content_type, "boundary=");
        if (boundary_start) {
            copy_value(msg->boundary, boundary_start, msg->content_type + strlen(msg->content_type), BOUNDARY_LEN);
        }
    }

    const char* auth_start = find_after(request_str, "Authorization: ");
    if (auth_start) {
        const char* auth_end = strstr(auth_start, "\r\n");
        copy_value(msg->authorization, auth_start, auth_end, AUTH_HEADER_LEN);
    }

    const char* len_start = find_after(request_str, "Content-Length: ");
    if (len_start) {
        msg->content_length = atoi(len_start);
    } else {
        msg->content_length = 0;
    }

    // 4. Find and Copy Body
    const char* body_start = strstr(request_str, "\r\n\r\n");
    if (body_start && msg->content_length > 0) {
        body_start += 4;

        int len_to_copy = msg->content_length;
        if (len_to_copy >= BODY_LEN) {
            len_to_copy = BODY_LEN - 1;
        }
        // For multipart, we copy the entire raw body including boundaries
        memcpy(msg->body, body_start, len_to_copy);
    }
}
