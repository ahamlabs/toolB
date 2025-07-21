//
// Created by sayan on 21/07/25.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/toolb_shm.h" // We need the constants from here
#include "include/ini.h"

// A struct to hold our configuration values
typedef struct {
    int port;
    const char* cert_file;
    const char* key_file;
    int timeout_seconds;
    int num_workers;
} AppConfig;

// This is the handler function that the INI parser will call for each line
static int handler(void* user, const char* section, const char* name, const char* value) {
    AppConfig* pconfig = (AppConfig*)user;

#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    if (MATCH("server", "port")) {
        pconfig->port = atoi(value);
    } else if (MATCH("server", "cert_file")) {
        pconfig->cert_file = strdup(value);
    } else if (MATCH("server", "key_file")) {
        pconfig->key_file = strdup(value);
    } else if (MATCH("server", "timeout_seconds")) {
        pconfig->timeout_seconds = atoi(value);
    } else if (MATCH("python_app", "num_workers")) {
        pconfig->num_workers = atoi(value);
    } else {
        return 0;  /* unknown section/name, error */
    }
    return 1;
}

// Global config variable
AppConfig config;

// Function to load the configuration from the file
void load_config(const char* filename) {
    // Set default values
    config.port = 8080;
    config.cert_file = "cert.pem";
    config.key_file = "key.pem";
    config.timeout_seconds = 30;
    config.num_workers = 4;

    if (ini_parse(filename, handler, &config) < 0) {
        printf("WARN: Can't load '%s', using default settings.\n", filename);
    }
}
