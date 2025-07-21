#ifndef __CONFIG_H__
#define __CONFIG_H__

// A struct to hold our configuration values
typedef struct {
    int port;
    const char* cert_file;
    const char* key_file;
    int timeout_seconds;
} AppConfig;

// Declare the global config variable so other files can use it
extern AppConfig config;

// Declare the function to load the configuration
void load_config(const char* filename);

#endif /* __CONFIG_H__ */
