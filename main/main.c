#include "main.h"

//hàm xử lý các event khi được phát sinh
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    //WIFI_EVENT_AP_STACONNECTED phát sinh -> log ra thông báo
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(TAG_WIFI, "Station "MACSTR" joined, AID = %d",
                 MAC2STR(event->mac), event->aid);
    } 
    //WIFI_EVENT_AP_STADISCONNECTED phát sinh -> log ra thông báo+viết ssid và pass vào flash + restart esp
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
        ESP_LOGI(TAG_WIFI, "Station "MACSTR" left, AID = %d, reason: %d",
                 MAC2STR(event->mac), event->aid, event->reason);
    } 
    //WIFI_EVENT_STA_START phát sinh -> bắt đầu thực hiện các phase connect
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } 
    //WIFI_EVENT_STA_DISCONNECTED phát sinh -> kiểm tra s_retry_num
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        //nếu s_retry_num < ESP_MAXIMUM_RETRY -> kết nối lại + log thông báo
        if (s_retry_num < ESP_MAXIMUM_RETRY) { 
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG_WIFI, "Retry to connect to the AP");
        } 
        //set WIFI_FAIL_BIT cho s_wifi_event_group + log thông báo + chuyển wifi sang AP mode
        else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGI(TAG_WIFI,"Connect to the AP fail");
            ap_start();
        }
    }
    //IP_EVENT_STA_GOT_IP phát sinh -> log thông báo + set WIFI_CONNECTED_BIT cho s_wifi_event_group
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG_WIFI, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        s_retry_num = 0;
    }
    else if(event_base == ESP_HTTP_SERVER_EVENT && event_id == HTTP_SERVER_EVENT_ERROR){
        printf("Something wrong!\n");
    }
}

//hàm init các thông tin liên quan đến wifi
void wifi_init(void)
{
    //khởi tạo LwIP và 1 LwIP task (TCP/IP task)
    ESP_ERROR_CHECK(esp_netif_init());

    //khởi tạo 1 vòng lặp sự kiện mặc định cho phép các system event được gửi đến event task
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    //
    esp_netif_sta = esp_netif_create_default_wifi_sta();
    esp_netif_ap = esp_netif_create_default_wifi_ap();

    //khởi tạo và cấu hình mặc định cho Wi-Fi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    //đăng ký hàm xử lý sự kiện cho WIFI_EVENT (ESP_EVENT_ANY_ID->mọi sự kiện)
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    //đăng ký hàm xử lý sự kiện cho IP_EVENT (IP_EVENT_STA_GOT_IP)
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
}

void station_start(char *ssid, char *password){
    ESP_LOGI(TAG_WIFI, "Station mode start!");
    s_wifi_event_group = xEventGroupCreate();

    esp_netif_set_default_netif(esp_netif_sta);

    //Wi-Fi configuration
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };

    strcpy((char*)wifi_config.sta.ssid, ssid);
    strcpy((char*)wifi_config.sta.password, password);

    //configure mode (Station mode) -> start
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG_WIFI, "wifi_init_sta finished.");


    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */

    //thông báo kết nối
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG_WIFI, "connected to ap SSID:%s password:%s",
                 ssid, password);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG_WIFI, "Failed to connect to SSID:%s, password:%s",
                 ssid, password);
    } else {
        ESP_LOGE(TAG_WIFI, "UNEXPECTED EVENT");
    }

    free(ssid);
    free(password);
}
void ap_start(void){
    //stop Wifi driver
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_LOGI(TAG_WIFI, "AP mode start!");

    esp_netif_set_default_netif(esp_netif_ap);

    //Wi-Fi configuration
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = ESP_WIFI_AP_SSID,
            .ssid_len = strlen(ESP_WIFI_AP_SSID),
            .channel = ESP_WIFI_CHANNEL,
            .password = ESP_WIFI_AP_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    if (strlen(ESP_WIFI_AP_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }    

    //configure mode (AP mode)-> start
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());    
}

void restart(void){
    //chờ 10s
    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    //restart esp
    esp_restart();
}

int8_t read_storage(char **ssid, char **password){
    int8_t flag = 1; //cờ đánh dấu đọc thành công (1 = thành công, 0 = không thành công)
    size_t size;
    nvs_handle_t my_handle;

    //mở NVS với namespace đã cho từ phân vùng NVS mặc định (namespace = storage) với chế độ chỉ đọc
    esp_err_t err = nvs_open("storage", NVS_READONLY, &my_handle);
    if(err != ESP_OK){
        ESP_LOGI(TAG_NVS, "Error %s opening nvs!", esp_err_to_name(err));
        nvs_close(my_handle);  
        return 0 ;
    }
    
    //lấy giá trị chuỗi cho khóa đã cho. Nếu key không tồn tại hoặc kiểu dữ liệu không 
    //khớp thì error sẽ được trả về. Trong trường hợp có lỗi thì out_value sẽ không được sửa đổi
    err = nvs_get_str(my_handle, "ssid", NULL, &size);
    if(err == ESP_OK){
        *ssid = (char*) malloc(size); //cấp phát bộ nhớ cho ssid 
        nvs_get_str(my_handle, "ssid", *ssid, &size);
    } else {
        flag = 0;
        ESP_LOGI(TAG_NVS, "Error %s reading ssid from nvs!", esp_err_to_name(err));
    }

    err = nvs_get_str(my_handle, "password", NULL, &size);
    if(err == ESP_OK){
        *password = (char*) malloc(size); //cấp phát bộ nhớ cho password
        nvs_get_str(my_handle, "password", *password, &size);
    } else {
        flag = 0;
        ESP_LOGI(TAG_NVS, "Error %s reading password from nvs!", esp_err_to_name(err));
    }   

    //đóng storage handle và giải phóng toàn bộ tài nguyên đươc phân bổ
    nvs_close(my_handle);

    printf("Read done! ssid: %s, password: %s\n", *ssid, *password);
    return flag;
}
void write_storage(char *ssid, char *password){
    nvs_handle_t my_handle;

    //mở NVS với namespace đã cho từ phân vùng NVS mặc định (namespace = storage) với chế độ đọc+ghi
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK){
        ESP_LOGI(TAG_NVS, "Error %s opening nvs!", esp_err_to_name(err));
    } else {
        //set string cho key.  Việc lưu trữ xâu dài có thể bị lỗi do bị phân mảnh bộ nhớ
        //bộ nhớ sẽ không được cập nhật cho đến khi hàm nvs_commit() được gọi
        ESP_ERROR_CHECK(nvs_set_str(my_handle, "ssid", ssid));
        ESP_ERROR_CHECK(nvs_set_str(my_handle, "password", password));

        //ghi bất kỳ thay đổi nào đang chờ xử lý vào NVS
        ESP_ERROR_CHECK(nvs_commit(my_handle));
        printf("Write done! ssid: %s, password: %s\n", ssid, password);
    }

    //đóng storage handle và giải phóng toàn bộ tài nguyên đươc phân bổ
    nvs_close(my_handle); 
}

//URI handler function được gọi trong request GET '/'
esp_err_t send_handler(httpd_req_t* req){
    printf("In send function\n");
    wifi_mode_t mode;
    esp_err_t err = esp_wifi_get_mode(&mode); //lấy mode của wifi
    if(err == ESP_OK){
        //thiết lập loại nội dung
        httpd_resp_set_type(req, "text/html");
        if(mode == WIFI_MODE_AP){
            //gửi dữ liệu như 1 HTTP response cho request
            err = httpd_resp_send(req, ap_html, strlen(ap_html));
        }
        else if(mode == WIFI_MODE_STA){
            err = httpd_resp_send(req, station_html, strlen(station_html));
        }
        else{
            //để gửi mã lỗi trong phản hồi cho HTTP request
            err = httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "WIFI MODE ERROR!");
            printf("Error in Wifi Mode\n");
        }
        printf("%s\n", esp_err_to_name(err));
        return ESP_OK;
    }
    return err;
}

//URI handler function được gọi trong request POST '/'
esp_err_t receive_handler(httpd_req_t* req){
    printf("In receive function\n");
    char buf[100];
    //đọc dữ liệu nội dung từ HTTP Request và bộ đệm đã cung cấp. 
    httpd_req_recv(req, buf, req->content_len);
    buf[req->content_len] = '\0';

    //chuỗi được gửi sẽ có dạng ssid&pasword -> tách để lấy ssid và password
    char *ssid = strtok(buf, "&");
    char *password = strtok(NULL, "&");
    printf("ssid: %s, password: %s\n", ssid, password);
    write_storage(ssid, password);
    restart();
    return ESP_OK;
}

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    printf("In 404 function\n");
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

//function for starting the webserver
httpd_handle_t setup_server(void){
    //Generate default configuration
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    //Empty handle to esp_http_server
    httpd_handle_t server = NULL;

    if(httpd_start(&server, &config) == ESP_OK){
        /* Register URI handlers */
        httpd_register_uri_handler(server, &uri_send);
        httpd_register_uri_handler(server, &uri_receive);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);

        ESP_ERROR_CHECK(esp_event_handler_instance_register(ESP_HTTP_SERVER_EVENT,
                                                            HTTP_SERVER_EVENT_ERROR,
                                                            &wifi_event_handler,
                                                            NULL,
                                                            NULL));
    }
    /* If server failed to start, handle will be NULL */
    return server;
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    char *ssid, *password;
    wifi_init();
    if(read_storage(&ssid, &password) == 0){
        ap_start();
    } else {
        station_start(ssid, password);
    }
    setup_server();
}