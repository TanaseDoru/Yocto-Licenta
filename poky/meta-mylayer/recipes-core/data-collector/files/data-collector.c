#include "data-collector.h"

#include <math.h>

static volatile int running = 1;
static volatile sig_atomic_t reload_config = 0;
static int sock_fd = -1;
static DataStore data_store = {.count = 0};
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
static FILE *log_fp = NULL;

void signal_handler(int signum) {
  if (signum == SIGHUP) {
    reload_config = 1;
  } else {
    printf("Received signal %d, shutting down...\n", signum);
    running = 0;
  }
}

void load_config(char *server_url) {
  FILE *config = fopen("/etc/data-collector/server.conf", "r");
  if (config) {
    if (fgets(server_url, 256, config)) {
      // Remove newline
      server_url[strcspn(server_url, "\n")] = 0;
      log_message("Configuration reloaded. New URL: %s", server_url);
    }
    fclose(config);
  } else {
    log_message("Failed to open config file for reload");
  }
}

void log_message(const char *fmt, ...) {
  va_list args;

  pthread_mutex_lock(&log_lock);

  if (!log_fp) {
    log_fp = fopen("/var/log/data-collector.log", "a");
  }

  va_start(args, fmt);
  if (log_fp) {
    vfprintf(log_fp, fmt, args);
    fprintf(log_fp, "\n");
    fflush(log_fp);
  } else {
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
  }
  va_end(args);

  pthread_mutex_unlock(&log_lock);
}

void cleanup() {
  if (sock_fd >= 0) {
    close(sock_fd);
    unlink(SOCKET_PATH);
  }
  if (log_fp) {
    fclose(log_fp);
    log_fp = NULL;
  }
  pthread_mutex_destroy(&lock);
  pthread_mutex_destroy(&log_lock);
  curl_global_cleanup();
}

// Callback for libcurl to handle response
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  // You can process server response here if needed
  return realsize;
}

int send_to_server(const char *json_data, const char *server_url) {
  CURL *curl;
  CURLcode res;
  int success = 0;

  curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "Error: curl_easy_init() failed\n");
    log_message("Error: curl_easy_init() failed");
  }
  if (curl) {
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, server_url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));
    } else {
      long response_code;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
      // 200 - OK || 201 - Created
      if (response_code == 200 || response_code == 201) {
        success = 1;
        printf("Data sent successfully\n");
      } else {
        fprintf(stderr, "Server returned code: %ld\n", response_code);
      }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
  }

  return success;
}

char *create_json_payload(DataStore *store) {
  size_t cap = BUFFER_SIZE * 4;
  char *json = malloc(cap);
  if (!json)
    return NULL;

  pthread_mutex_lock(&store->lock);

  int offset =
      snprintf(json, cap, "{\"timestamp\":%ld,\"sensors\":[", time(NULL));
  if (offset < 0 || (size_t)offset >= cap) {
    pthread_mutex_unlock(&store->lock);
    free(json);
    return NULL;
  }

  for (int i = 0; i < store->count; i++) {
    int wrote =
        snprintf(json + offset, cap - (size_t)offset,
                 "{\"name\":\"%s\",\"value\":%.2f,\"timestamp\":%ld}%s",
                 store->data[i].sensor_name, store->data[i].value,
                 store->data[i].timestamp, (i < store->count - 1) ? "," : "");
    if (wrote < 0 || (size_t)wrote >= cap - (size_t)offset) {
      log_message("Warning: JSON payload truncated at %d sensors", i);
      break;
    }
    offset += wrote;
  }

  snprintf(json + offset, cap - (size_t)offset, "]}");

  pthread_mutex_unlock(&store->lock);

  return json;
}

void *socket_listener(void *arg) {
  struct sockaddr_un addr;
  char buffer[BUFFER_SIZE];

  // Create socket
  sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
  if (sock_fd < 0) {
    perror("socket");
    return NULL;
  }

  // Remove old socket file if exists
  unlink(SOCKET_PATH);

  // Bind socket
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

  if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(sock_fd);
    return NULL;
  }

  // Set socket permissions so other processes can write
  chmod(SOCKET_PATH, 0666);

  printf("Socket listener started on %s\n", SOCKET_PATH);
  log_message("Socket listener started on %s", SOCKET_PATH);

  while (running) {
    ssize_t n = recvfrom(sock_fd, buffer, BUFFER_SIZE - 1, 0, NULL, NULL);
    if (n > 0) {
      buffer[n] = '\0';

      // Parse incoming data: format "sensor_name:value"
      char sensor_name[64];
      float value;

      if (sscanf(buffer, "%63[^:]:%f", sensor_name, &value) == 2) {
        if (!isfinite(value)) {
          fprintf(stderr, "Warning: invalid sensor value: %s\n", buffer);
          log_message("Warning: invalid sensor value: %s", buffer);
          continue;
        }
        pthread_mutex_lock(&data_store.lock);

        if (data_store.count < MAX_SENSORS) {

          // Add new entry
          strncpy(data_store.data[data_store.count].sensor_name, sensor_name,
                  sizeof(data_store.data[0].sensor_name) - 1);
          data_store.data[data_store.count]
              .sensor_name[sizeof(data_store.data[0].sensor_name) - 1] = '\0';
          data_store.data[data_store.count].value = value;
          data_store.data[data_store.count].timestamp = time(NULL);
          data_store.count++;

          printf("Received: %s = %.2f (Total in queue: %d)\n", sensor_name,
                 value, data_store.count);
          log_message("Received: %s = %.2f (Total in queue: %d)", sensor_name,
                      value, data_store.count);
        } else {
          fprintf(stderr, "Warning: Data store is full. Dropping message: %s\n",
                  buffer);
          log_message("Warning: Data store is full. Dropping message: %s",
                      buffer);
        }

        pthread_mutex_unlock(&data_store.lock);
      }
    }
  }

  return NULL;
}

void *web_sender(void *arg) {
  char *server_url = (char *)arg;
  // Make a local copy to allow modification safely if needed,
  // but since we are modifying the pointer buffer passed from main which is
  // explicitly for this, we need to be careful about thread safety if other
  // threads read it. In this app, only web_sender reads it after
  // initialization. Exception: main prints it at startup.

  // Actually, 'arg' points to 'server_url' in 'main'.
  // 'main' joins threads, so stack memory is valid.
  // However, we need to ensure we don't write slightly out of bounds if size
  // changes (checked in load_config).

  printf("Web sender started, sending to: %s\n", server_url);
  log_message("Web sender started, sending to: %s", server_url);

  while (running) {
    if (reload_config) {
      load_config(server_url);
      reload_config = 0;
      printf("Web sender reload: %s\n", server_url);
    }

    sleep(SEND_INTERVAL);

    if (data_store.count > 0) {
      char *json = create_json_payload(&data_store);
      if (json) {
        printf("Sending data: %s\n", json);
        log_message("Sending data: %s", json);
        if (send_to_server(json, server_url)) {
          pthread_mutex_lock(&data_store.lock);
          data_store.count = 0;
          pthread_mutex_unlock(&data_store.lock);
        }
        free(json);
      }
    } else {
      printf("No sensor data to send yet\n");
      log_message("No sensor data to send yet");
    }
  }

  return NULL;
}

int main(int argc, char *argv[]) {
  pthread_t listener_thread, sender_thread;
  char server_url[256] = "http://10.204.243.197/data";

  // Read server URL from config file
  FILE *config = fopen("/etc/data-collector/server.conf", "r");
  if (config) {
    if (fgets(server_url, sizeof(server_url), config)) {
      // Remove newline
      server_url[strcspn(server_url, "\n")] = 0;
    }
    fclose(config);
  }

  // Setup signal handlers
  // Setup signal handlers
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGHUP, signal_handler);

  // Initialize
  pthread_mutex_init(&data_store.lock, NULL);
  curl_global_init(CURL_GLOBAL_ALL);

  printf("Starting data collector daemon...\n");
  printf("Server URL: %s\n", server_url);
  log_message("Starting data collector daemon...");
  log_message("Server URL: %s", server_url);

  // Start threads
  if (pthread_create(&listener_thread, NULL, socket_listener, NULL) != 0) {
    perror("pthread_create listener");
    cleanup();
    return 1;
  }

  if (pthread_create(&sender_thread, NULL, web_sender, server_url) != 0) {
    perror("pthread_create sender");
    cleanup();
    return 1;
  }

  // Wait for threads
  pthread_join(listener_thread, NULL);
  pthread_join(sender_thread, NULL);

  cleanup();
  printf("Data collector stopped\n");
  log_message("Data collector stopped");

  return 0;
}
