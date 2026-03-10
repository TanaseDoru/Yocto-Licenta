#ifndef DATA_COLLECTOR_H
#define DATA_COLLECTOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <time.h>
#include <curl/curl.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>

#define SOCKET_PATH "/tmp/sensor_data.sock"
#define BUFFER_SIZE 1024
#define MAX_SENSORS 25
#define SEND_INTERVAL 10
#define LOCAL_FALLBACK_PATH "/var/spool/data-collector/unsent.jsonl"
#define RETRY_COUNT 3
#define RETRY_DELAY_SEC 2

typedef struct {
    char sensor_name[64];
    float value;
    time_t timestamp;
} SensorData;

typedef struct {
    SensorData data[MAX_SENSORS];
    int count;
    pthread_mutex_t lock;
} DataStore;

// Function declarations
void* socket_listener(void* arg);
void* web_sender(void* arg);
int send_to_server(const char* json_data, const char* server_url);
char* create_json_payload(DataStore* store);
void signal_handler(int signum);
void cleanup();
void log_message(const char *fmt, ...);


int send_with_retry_or_store(const char *json_data, const char *server_url);
int store_locally(const char *json_data);

#endif
