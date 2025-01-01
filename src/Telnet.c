/*
 * SmartMeter.c
 */

#include "Telnet.h"
#include "Telnet_Config.h"

#include "System.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/portmacro.h"
#include "xtensa_api.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>

#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

static const char TAG[] = "TELNET";

// https://github.com/espressif/esp-idf/blob/master/examples/protocols/sockets/tcp_server/main/tcp_server.c
// https://github.com/espressif/esp-idf/blob/master/examples/protocols/sockets/tcp_server/main/Kconfig.projbuild
// https://esp32.com/viewtopic.php?t=911

typedef struct __attribute__((__packed__)) {
    int listen_sock;
    int sock;
    struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
    int port;
} telnet_t;

static void Telnet_tcp_server_task(void *pvParameters);

void Telnet_Initialize(void) {
    xTaskCreate(Telnet_tcp_server_task, "tcp_server_task", 4096, NULL, 5, NULL);
}

#if defined(TELNET_EXAMPLE_SEND_ONLY)
static void Telnet_client_task(void *pvParameters)
{
    telnet_t telnet;
    memcpy(&telnet, pvParameters, sizeof(telnet));
    
    char addr_str[128];
    char tx_buffer[128];

    // Convert ip address to string
    if (telnet.source_addr.ss_family == PF_INET) {
        inet_ntoa_r(((struct sockaddr_in *)&telnet.source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
    }

    ESP_LOGI(TAG, "Socket %i accepted ip address: %s", telnet.sock, addr_str);

    do {
        // send() can return less bytes than supplied length.
        tx_buffer[0] = 'T';
        tx_buffer[1] = 'e';
        tx_buffer[2] = 'l';
        tx_buffer[3] = 'n';
        tx_buffer[4] = 'e';
        tx_buffer[5] = 't';
        tx_buffer[6] = ' ';
        int to_write = 7;
        while (to_write > 0) {
            int written = send(telnet.sock, tx_buffer + (7 - to_write), to_write, 0);
            if (written < 0) {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                shutdown(telnet.sock, 0);
                close(telnet.sock);
                ESP_LOGW(TAG, "Socket %i closed", telnet.sock);
                vTaskDelete(NULL);
                return;
            }
            to_write -= written;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    } while (1);
}
#else
static void Telnet_client_task(void *pvParameters)
{
    telnet_t telnet;
    memcpy(&telnet, pvParameters, sizeof(telnet));

    char addr_str[128];
    int len;
    char rx_buffer[128];
    char tx_buffer[128];

    // Convert ip address to string
    if (telnet.source_addr.ss_family == PF_INET) {
        inet_ntoa_r(((struct sockaddr_in *)&telnet.source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
    }

    ESP_LOGI(TAG, "Socket %i accepted ip address: %s", telnet.sock, addr_str);

    do {
        len = recv(telnet.sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
            shutdown(telnet.sock, 0);
            close(telnet.sock);
            ESP_LOGW(TAG, "Socket %i closed", telnet.sock);
            vTaskDelete(NULL);
            return;
        } else if (len == 0) {
            shutdown(telnet.sock, 0);
            close(telnet.sock);
            ESP_LOGW(TAG, "Socket %i closed", telnet.sock);
            vTaskDelete(NULL);
            return;
        } else {
            rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
            ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);

            // send() can return less bytes than supplied length.
            int to_write = len;
            while (to_write > 0) {
                int written = send(telnet.sock, rx_buffer + (len - to_write), to_write, 0);
                if (written < 0) {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                    //Telnet_SetStatus(i, Telnet_StatusDisconnect);
                    shutdown(telnet.sock, 0);
                    close(telnet.sock);
                    ESP_LOGW(TAG, "Socket %i closed", telnet.sock);
                    vTaskDelete(NULL);
                    return;
                }
                to_write -= written;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    } while (1);
}
#endif

static void Telnet_tcp_server_task(void *pvParameters)
{
    int addr_family = AF_INET;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = TELNET_KEEPALIVE_IDLE;
    int keepInterval = TELNET_KEEPALIVE_INTERVAL;
    int keepCount = TELNET_KEEPALIVE_COUNT;

    telnet_t telnet;

    struct sockaddr_storage dest_addr;
    struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
    dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4->sin_family = AF_INET;
    dest_addr_ip4->sin_port = htons(TELNET_PORT);
    ip_protocol = IPPROTO_IP;
    telnet.port = TELNET_PORT;
    
    telnet.listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (telnet.listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create listen socket %i: errno %d", telnet.listen_sock, errno);
        vTaskDelete(NULL);
        return;
    }
    
    int opt = 1;
    setsockopt(telnet.listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    ESP_LOGI(TAG, "Listen socket %i created", telnet.listen_sock);

    int err = bind(telnet.listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Listen socket %i unable to bind: errno %d", telnet.listen_sock, errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        close(telnet.listen_sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Listen socket %i bound, port %d", telnet.listen_sock, TELNET_PORT);

    err = listen(telnet.listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen socket %i: errno %d", telnet.listen_sock, errno);
        close(telnet.listen_sock);
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        ESP_LOGI(TAG, "Socket listening for new connection");
        socklen_t addr_len = sizeof(telnet.source_addr);
        telnet.sock = accept(telnet.listen_sock, (struct sockaddr *)&telnet.source_addr, &addr_len);
        
        if (telnet.sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Set tcp keepalive option
        setsockopt(telnet.sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(telnet.sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(telnet.sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(telnet.sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
                
        xTaskCreate(Telnet_client_task, "tcp_server_task", 4096, (void *)&telnet, 10, NULL);
        
        vTaskDelay(pdMS_TO_TICKS(20));
    }  
}