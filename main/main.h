#ifndef __MAIN_H
#define __MAIN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h> //Requires by memset
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/ip4_addr.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_http_server.h"

#define ESP_MAXIMUM_RETRY       5           //số lần kết nối wifi lại tối đa

#define ESP_WIFI_AP_SSID        "Nqd"   //ssid của wifi khi ở AP mode
#define ESP_WIFI_AP_PASS        "123456789@"  //password của wifi khi ở AP mode
#define ESP_WIFI_CHANNEL        1           
#define MAX_STA_CONN            5           //số station có thể kết nối cùng lúc


#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER ""
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG_WIFI = "wifi";
static const char *TAG_NVS = "nvs";
static int s_retry_num = 0;

esp_netif_t *esp_netif_sta;
esp_netif_t *esp_netif_ap;

const char* ap_html = 
"<!DOCTYPE html>\n"
"<html lang=\"vi\">\n"
"  <head>\n"
"    <meta charset=\"UTF-8\" />\n"
"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\n"
"    <title>ESP32 Websever</title>\n"
"\n"
"    <style>\n"
"      body {\n"
"        background-image: url(https://th.bing.com/th/id/R.6589ea7fa7f394455446185614e629c1?rik=R77ZV%2bmDudJd0w&pid=ImgRaw&r=0);\n"
"        background-size: cover;\n"
"        background-repeat: no-repeat;\n"
"        background-position: top;\n"
"        font-family: Arial, sans-serif;\n"
"        display: flex;\n"
"        justify-content: center;\n"
"        align-items: center;\n"
"        height: 100vh;\n"
"        margin: auto;\n"
"      }\n"
"      .container {\n"
"        background-color: rgba(255, 255, 255, 0.568);\n"
"        backdrop-filter: blur(5px);\n"
"        border-radius: 8px;\n"
"        box-shadow: 0 0 30px rgba(0, 0, 0);\n"
"        width: 300px;\n"
"        padding: 25px;\n"
"      }\n"
"      h1 {\n"
"        margin: auto;\n"
"        font-size: 24;\n"
"        text-align: center;\n"
"      }\n"
"      h2 {\n"
"        margin: 15px 0;\n"
"        font-size: 18;\n"
"        color: #555;\n"
"        text-align: center;\n"
"      }\n"
"      form {\n"
"        display: flex;\n"
"        flex-direction: column;\n"
"      }\n"
"      label {\n"
"        margin-bottom: 5px;\n"
"        text-align: left;\n"
"      }\n"
"      input[type=\"text\"],\n"
"      input[type=\"password\"] {\n"
"        width: 100%;\n"
"        padding: 8px;\n"
"        margin: 8px 0 15px 0;\n"
"        box-sizing: border-box;\n"
"        border: 1px solid #ccc;\n"
"        border-radius: 4px;\n"
"      }\n"
"      input[type=\"submit\"] {\n"
"        background-color: #4c98af;\n"
"        color: white;\n"
"        padding: 10px 15px;\n"
"        border: none;\n"
"        border-radius: 4px;\n"
"        cursor: pointer;\n"
"        margin-top: 10px;\n"
"      }\n"
"      input[type=\"submit\"]:hover {\n"
"        background-color: #4cafaf;\n"
"      }\n"
"    </style>\n"
"  </head>\n"
"  <body>\n"
"    <div class=\"container\">\n"
"      <h1>ESP32</h1>\n"
"      <h1>Websever</h1>\n"
"      <h2>AiThings Lab IoT-07</h2>\n"
"      <label for=\"ssid\">SSID</label>\n"
"      <input type=\"text\" id=\"ssid\" name=\"SSID\" placeholder=\"SSID\" required />\n"
"      <label for=\"password\">Password</label>\n"
"      <input\n"
"        type=\"password\"\n"
"        id=\"password\"\n"
"        name=\"Password\"\n"
"        placeholder=\"password\"\n"
"        required\n"
"      />\n"
"      <input type=\"submit\" id=\"button\" value=\"submit\" onclick=\"myFunction()\" />\n"
"    </div>\n"
"    <script>\n"
"      var xhttp1 = new XMLHttpRequest();\n"
"\n"
"      function myFunction() {\n"
"        var element1 = document.getElementById(\"ssid\");\n"
"        var element2 = document.getElementById(\"password\");\n"
"\n"
"        if (element1.value != "" && element2.value != "") {\n"
"          xhttp1.open(\"POST\", \"/\", true);\n"
"          xhttp1.send(element1.value + \"&\" + element2.value);\n"
"        }\n"
"      }\n"
"    </script>\n"
"  </body>\n"
"</html>\n" ;

const char* station_html = 
"<!DOCTYPE html>\n"
"<html lang=\"vi\">\n"
"  <head>\n"
"    <meta charset=\"UTF-8\" />\n"
"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\n"
"    <title>ESP32 Websever</title>\n"
"\n"
"    <style>\n"
"      body {\n"
"        background-image: url(https://th.bing.com/th/id/R.6589ea7fa7f394455446185614e629c1?rik=R77ZV%2bmDudJd0w&pid=ImgRaw&r=0);\n"
"        background-size: cover;\n"
"        background-repeat: no-repeat;\n"
"        background-position: top;\n"
"        font-family: Arial, sans-serif;\n"
"        display: flex;\n"
"        justify-content: center;\n"
"        align-items: center;\n"
"        height: 100vh;\n"
"        margin: auto;\n"
"      }\n"
"      .container {\n"
"        background-color: rgba(255, 255, 255, 0.568);\n"
"        backdrop-filter: blur(5px);\n"
"        border-radius: 8px;\n"
"        box-shadow: 0 0 30px rgba(0, 0, 0);\n"
"        width: 300px;\n"
"        padding: 25px;\n"
"      }\n"
"      h1 {\n"
"        margin: auto;\n"
"        font-size: 24;\n"
"        text-align: center;\n"
"      }\n"
"      h2 {\n"
"        margin: 15px 0;\n"
"        font-size: 18;\n"
"        color: #555;\n"
"        text-align: center;\n"
"      }\n"
"      h3 {\n"
"        background-color: #4c98af88;\n"
"        color: white;\n"
"        padding: 10px 15px;\n"
"        border: none;\n"
"        border-radius: 4px;\n"
"        margin-top: 10px;\n"
"        text-align: center;\n"
"      }\n"
"    </style>\n"
"  </head>\n"
"  <body>\n"
"    <div class=\"container\">\n"
"      <h1>ESP32</h1>\n"
"      <h1>Websever</h1>\n"
"      <h2>AiThings Lab IoT-07</h2>\n"
"      <h3>Station mode</h3>\n"
"    </div>\n"
"  </body>\n"
"</html>\n";

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
void wifi_init(void);
void restart(void);
void ap_start(void);
void station_start(char *ssid, char *password);

int8_t read_storage(char **ssid, char **password);
void write_storage(char *ssid, char *password);

esp_err_t send_handler(httpd_req_t* req);
esp_err_t receive_handler(httpd_req_t* req);
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err);
httpd_handle_t setup_server(void);

//URI handler structure cho GET '/'
httpd_uri_t uri_send = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = send_handler,
    .user_ctx = NULL,
};

//URI handler structure cho POST '/'
httpd_uri_t uri_receive = {
    .uri = "/",
    .method = HTTP_POST,
    .handler = receive_handler,
    .user_ctx = NULL,
};

#endif