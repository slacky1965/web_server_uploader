#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <math.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "cJSON.h"

#include "http.h"
#include "utils.h"

/* Buffer for OTA and another load or read from spiffs */
#define OTA_BUF_LEN	 1024

/* Defined upload path */
#define PATH_HTML   "/html/"
#define PATH_IMAGE  "/image/"
#define PATH_UPLOAD "/upload/"

/* Legal URL web server */
#define	URL 		"/*"
#define ROOT        "/"
#define INDEX       "/index.html"
#define UPLOAD      "/upload/*"
#define LIST        "/list"

static char *TAG = "web_server_http";

static char *webserver_html_path = NULL;

static esp_err_t webserver_response(httpd_req_t *req);
static esp_err_t webserver_upload(httpd_req_t *req);
static esp_err_t webserver_list(httpd_req_t *req);

static const httpd_uri_t uri_html = {
        .uri = URL,
        .method = HTTP_GET,
        .handler = webserver_response };

static const httpd_uri_t upload_html = {
        .uri = UPLOAD,
        .method = HTTP_POST,
        .handler = webserver_upload };

static const httpd_uri_t list_html = {
        .uri = LIST,
        .method = HTTP_POST,
        .handler = webserver_list };

static void reboot_task(void *pvParameter) {

    vTaskDelay(3000 / portTICK_PERIOD_MS);
    esp_restart();
    for(;;);
    vTaskDelete(NULL);
}

static char* http_content_type(char *path) {
    char *ext = strrchr(path, '.');
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0)  return "text/css";
    if (strcmp(ext, ".js") == 0)   return "text/javascript";
    if (strcmp(ext, ".png") == 0)  return "image/png";
    if (strcmp(ext, ".jpg") == 0)  return "image/jpeg";
    if (strcmp(ext, ".ico") == 0)  return "image/x-icon";
    if (strcmp(ext, ".json") == 0) return "application/json";
    return "text/plain";
}

static esp_err_t webserver_read_file(httpd_req_t *req) {

    char buff[OTA_BUF_LEN];
    size_t read_len;

    sprintf(buff, "%s%s", webserver_html_path, req->uri);

    FILE *f = fopen(buff, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Cannot open file %s. (%s:%u)", buff, __FILE__, __LINE__);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
        return ESP_FAIL;
    }

    char *type = http_content_type(buff);
    httpd_resp_set_type(req, type);

    do {
        read_len = fread(buff, 1, sizeof(buff), f);
        if (read_len > 0) httpd_resp_send_chunk(req, buff, read_len);
    } while(read_len == sizeof(buff));

    httpd_resp_sendstr_chunk(req, NULL);

    fclose(f);

    return ESP_OK;
}

static esp_err_t webserver_response(httpd_req_t *req) {

    ESP_LOGI(TAG, "Request URI \"%s\"", req->uri);

    if (strcmp(req->uri, ROOT) == 0) {
        strcpy((char*) req->uri, INDEX);
    }

    esp_err_t ret = webserver_read_file(req);

    httpd_resp_sendstr_chunk(req, NULL);

    return ret;
}

static esp_err_t webserver_upload_html(httpd_req_t *req, const char *full_name) {

    FILE *fp = NULL;
    size_t global_cont_len, recorded_len = 0;
    int received;
    char *tmpname, *newname, *name;
    char *err = "Unknown error";
    char buf[MAX_BUFF_RW];

    global_cont_len = req->content_len;

    if (!get_status_spiffs()) {
        err = "Spiffs not mount";
        ESP_LOGE(TAG, "%s. (%s:%u)", err, __FILE__, __LINE__);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err);
        return ESP_FAIL;
    }

	if (req->content_len == 0) {
        err = "File empty";
        ESP_LOGE(TAG, "%s. (%s:%u)", err, __FILE__, __LINE__);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err);
        return ESP_FAIL;
	}

    if (get_fs_free_space() < req->content_len) {
        err = "Upload file too large";
        ESP_LOGE(TAG, "%s. (%s:%u)", err, __FILE__, __LINE__);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err);
        return ESP_FAIL;
    }

    tmpname = malloc(strlen(MOUNT_POINT_SPIFFS) + 1 + strlen(full_name) + 5);

    if (!tmpname) {
        err = "Error allocation memory";
        ESP_LOGE(TAG, "%s. (%s:%u)", err, __FILE__, __LINE__);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err);
        return ESP_FAIL;
    }

    newname = malloc(strlen(MOUNT_POINT_SPIFFS) + 1 + strlen(full_name) + 1);
    if (!newname) {
        free(tmpname);
        err = "Error allocation memory";
        ESP_LOGE(TAG, "%s. (%s:%u)", err, __FILE__, __LINE__);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err);
        return ESP_FAIL;
    }

    sprintf(newname, "%s%s", MOUNT_POINT_SPIFFS, full_name);

    sprintf(tmpname, "%s%s", newname, ".tmp");

    fp = fopen(tmpname, "wb");

    if (!fp) {
        err = "Failed to create file";
        ESP_LOGE(TAG, "%s \"%s\" (%s:%u)", err, tmpname, __FILE__, __LINE__);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err);
        return ESP_FAIL;
    }

    fseek(fp, 0, SEEK_SET);

    printf("Loading \"%s\" file\n", full_name);
    printf("Please wait\n");

    while(global_cont_len) {
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, buf, MIN(global_cont_len, MAX_BUFF_RW))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry if timeout occurred */
                continue;
            }

            /* In case of unrecoverable error,
             * close and delete the unfinished file*/
            fclose(fp);
            unlink(tmpname);

            err = "File reception failed";
            ESP_LOGE(TAG, "%s. (%s:%u)", err, __FILE__, __LINE__);
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err);
            free(newname);
            free(tmpname);
            return ESP_FAIL;
        }

        recorded_len += received;

        /* Write buffer content to file on storage */
        if (received && (received != fwrite(buf, 1, received, fp))) {
            /* Couldn't write everything to file!
             * Storage may be full? */
            fclose(fp);
            unlink(tmpname);
            free(newname);
            free(tmpname);

            err = "Failed to write file to storage";
            ESP_LOGE(TAG, "%s. (%s:%u)", err, __FILE__, __LINE__);
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err);
            return ESP_FAIL;
        }

        /* Keep track of remaining size of
         * the file left to be uploaded */
        global_cont_len -= received;

        printf(".");
        fflush(stdout);
    }


    fclose(fp);

    printf("\n");

    printf("File transferred finished: %d bytes\n", recorded_len);

    struct stat st;
    if (stat(newname, &st) == 0) {
        unlink(newname);
    }

    if (rename(tmpname, newname) != 0) {
        ESP_LOGE(TAG, "File rename \"%s\" to \"%s\" failed. (%s:%u)", tmpname, newname, __FILE__, __LINE__);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to rename file");
        free(newname);
        free(tmpname);
        return ESP_FAIL;
    }

    name = strrchr (full_name, DELIM_CHR);

    if (name) name++;

    sprintf(buf, "File `%s` %d bytes uploaded successfully.", name?name:full_name, recorded_len);
    httpd_resp_send(req, buf, strlen(buf));

    free(tmpname);
    free(newname);

    return ESP_OK;
}

static esp_err_t webserver_update(httpd_req_t *req, const char *full_name) {

    const esp_partition_t *partition;
    esp_ota_handle_t ota_handle;
    esp_err_t ret = ESP_OK;

    size_t global_cont_len;
    size_t len;
    size_t global_recv_len = 0;

    char buf[OTA_BUF_LEN];
    char *err = "Unknown error";
    char *name;

    esp_image_header_t          *image_header = NULL;
    esp_app_desc_t              *app_desc = NULL;

    global_cont_len = req->content_len;

	if (req->content_len == 0) {
        err = "File empty";
        ESP_LOGE(TAG, "%s. (%s:%u)", err, __FILE__, __LINE__);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err);
        return ESP_FAIL;
	}

    partition = esp_ota_get_next_update_partition(NULL);

    if (partition) {
        if (partition->size < req->content_len) {
            err = "Firmware image too large";
            ESP_LOGE(TAG, "%s. (%s:%u)", err, __FILE__, __LINE__);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err);
            return ESP_FAIL;
        }

        ret = esp_ota_begin(partition, OTA_SIZE_UNKNOWN, &ota_handle);
        if (ret == ESP_OK) {
            bool begin = true;
            while(global_cont_len) {
                len = httpd_req_recv(req, buf, MIN(global_cont_len, OTA_BUF_LEN));
                if (len) {
                    if (begin) {
                        begin = false;
                        image_header = (esp_image_header_t*)buf;
                        app_desc = (esp_app_desc_t*)(buf +
                                    sizeof(esp_image_header_t) +
                                    sizeof(esp_image_segment_header_t));
                        if (image_header->magic != ESP_IMAGE_HEADER_MAGIC ||
                            app_desc->magic_word != ESP_APP_DESC_MAGIC_WORD) {
                            err = "Invalid flash image type";
                            ESP_LOGE(TAG, "%s. (%s:%u)", err, __FILE__, __LINE__);
                            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err);
                            return ESP_FAIL;
                        }
                        char *filename = strrchr(req->uri, DELIM_CHR);
                        if (filename) {
                            printf("Uploading image file \"%s\"\n", filename+1);
                        }
                        printf("Image project name \"%s\"\n", app_desc->project_name);
                        printf("Compiled %s %s\n", app_desc->time, app_desc->date);
                        printf("IDF version %s\n", app_desc->idf_ver);
                        printf("Writing to partition name \"%s\" subtype %d at offset 0x%x\n",
                              partition->label, partition->subtype, partition->address);
                        printf("Please wait\n");
                        vTaskDelay(1000 / portTICK_PERIOD_MS);
                    }
                    ret = esp_ota_write(ota_handle, (void*)buf, len);
                    if (ret != ESP_OK) {
                        switch (ret) {
                            case ESP_ERR_INVALID_ARG:
                                err = "Handle is invalid";
                                break;
                            case ESP_ERR_OTA_VALIDATE_FAILED:
                                err = "First byte of image contains invalid app image magic byte";
                                break;
                            case ESP_ERR_FLASH_OP_TIMEOUT:
                            case ESP_ERR_FLASH_OP_FAIL:
                                err = "Flash write failed";
                                break;
                            case ESP_ERR_OTA_SELECT_INFO_INVALID:
                                err = "OTA data partition has invalid contents";
                                break;
                            default:
                                err = "Unknown error";
                                break;
                        }

                        ESP_LOGE(TAG, "OTA write return error. %s. (%s:%d)", err, __FILE__, __LINE__);
                        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err);
                        return ESP_FAIL;
                    }
                    global_recv_len += len;
                    global_cont_len -= len;
                    printf(".");
                    fflush(stdout);
                }
            }
            printf("\n");
            printf("Binary transferred finished: %d bytes\n", global_recv_len);

            ret = esp_ota_end(ota_handle);
            if (ret != ESP_OK) {
                switch (ret) {
                    case ESP_ERR_NOT_FOUND:
                        err = "OTA handle was not found";
                        break;
                    case ESP_ERR_INVALID_ARG:
                        err = "Handle was never written to";
                        break;
                    case ESP_ERR_OTA_VALIDATE_FAILED:
                        err = "OTA image is invalid";
                        break;
                    case ESP_ERR_INVALID_STATE:
                        err = "Internal error writing the final encrypted bytes to flash";
                }

                ESP_LOGE(TAG, "OTA end return error. %s. (%s:%d)", err, __FILE__, __LINE__);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err);
                return ESP_FAIL;
            }
            ret = esp_ota_set_boot_partition(partition);
            if (ret != ESP_OK) {
                err = "Set boot partition is error";
                ESP_LOGE(TAG, "%s. (%s:%u)", err, __FILE__, __LINE__);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err);
                return ESP_FAIL;
            }

            name = strrchr (full_name, DELIM_CHR);

            if (name) name++;

            sprintf(buf, "File `%s` %d bytes uploaded successfully.\nNext boot partition is %s.\nRestart system...", name?name:full_name, global_recv_len, partition->label);
            httpd_resp_send(req, buf, strlen(buf));

            const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
            printf("Next boot partition \"%s\" name subtype %d at offset 0x%x\n",
                  boot_partition->label, boot_partition->subtype, boot_partition->address);
            printf("Prepare to restart system!\n");
            printf("Rebooting...\n");

            xTaskCreate(&reboot_task, "reboot_task", 2048, NULL, 0, NULL);


        } else {

            switch (ret) {
                case ESP_ERR_INVALID_ARG:
                    err = "Partition or out_handle arguments were NULL, or not OTA app partition";
                    break;
                case ESP_ERR_NO_MEM:
                    err = "Cannot allocate memory for OTA operation";
                    break;
                case ESP_ERR_OTA_PARTITION_CONFLICT:
                    err = "Partition holds the currently running firmware, cannot update in place";
                    break;
                case ESP_ERR_NOT_FOUND:
                    err = "Partition argument not found in partition table";
                    break;
                case ESP_ERR_OTA_SELECT_INFO_INVALID:
                    err = "The OTA data partition contains invalid data";
                    break;
                case ESP_ERR_INVALID_SIZE:
                    err = "Partition doesn’t fit in configured flash size";
                    break;
                case ESP_ERR_FLASH_OP_TIMEOUT:
                case ESP_ERR_FLASH_OP_FAIL:
                    err = "Flash write failed";
                    break;
                case ESP_ERR_OTA_ROLLBACK_INVALID_STATE:
                    err = "The running app has not confirmed state";
                    break;
                default:
                    err = "Unknown error";
                    break;
            }
            ESP_LOGE(TAG, "OTA begin return error. %s. (%s:%u)", err, __FILE__, __LINE__);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err);
            return ESP_FAIL;

        }


    } else {
        err = "No partiton";
        ESP_LOGE(TAG, "%s. (%s:%u)", err, __FILE__, __LINE__);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err);
        return ESP_FAIL;
    }

    return ESP_OK;
}


static esp_err_t webserver_upload(httpd_req_t *req) {

    const char *full_path;
    char *err = NULL;

    full_path = req->uri+strlen(PATH_UPLOAD)-1;

    if (strncmp(full_path, PATH_HTML, strlen(PATH_HTML)) == 0) {
        if (strlen(full_path+strlen(PATH_HTML)) >= CONFIG_FATFS_MAX_LFN) {
            err = "Filename too long";
            ESP_LOGE(TAG, "%s. (%s:%u)", err, __FILE__, __LINE__);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err);
            return ESP_FAIL;
        }

        return webserver_upload_html(req, full_path);

    } else if (strncmp(full_path, PATH_IMAGE, strlen(PATH_IMAGE)) == 0) {
        if (strlen(full_path+strlen(PATH_IMAGE)) >= CONFIG_FATFS_MAX_LFN) {
            err = "Filename too long";
            ESP_LOGE(TAG, "%s. (%s:%u)", err, __FILE__, __LINE__);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err);
            return ESP_FAIL;
        }

        return webserver_update(req, full_path);

    } else {
        err = "Invalid path";
        ESP_LOGE(TAG, "%s: %s. (%s:%u)", err, req->uri, __FILE__, __LINE__);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err);
        return ESP_FAIL;
    }


    return ESP_OK;
}

static esp_err_t webserver_list(httpd_req_t *req) {

    DIR *dir;
    struct dirent *de;
    char *err = NULL;
    char buff[512];
    char spaces[16];

    FILE *fp;
    struct stat file_stat;
    size_t len, total_len;
    dir = opendir(webserver_html_path);

    if (!dir) {
        sprintf(buff, "Open \"%s\" directory failed", webserver_html_path);
        ESP_LOGE(TAG, "%s. (%s:%u)", buff, __FILE__, __LINE__);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, buff);
        return ESP_FAIL;
    }

    sprintf(buff, "Directory: %s\n\n", webserver_html_path);
    httpd_resp_sendstr_chunk(req, buff);

    total_len = 0;

    while ((de = readdir(dir))) {
        sprintf(buff, "%s%s%s", webserver_html_path, DELIM, de->d_name);
        fp = fopen(buff, "rb");
        if (fp == NULL) {
            err = "Cannot open file";
            ESP_LOGE(TAG, "%s %s. (%s:%u)", err, de->d_name, __FILE__, __LINE__);
            sprintf(buff, "%s\n", de->d_name);
        } else {
            if (fstat(fileno(fp), &file_stat) == 0) {
                sprintf(buff, "%ld", file_stat.st_size);
                len = 7-strlen(buff);
                memset(spaces, ' ', len);
                spaces[len] = 0;
                sprintf(buff, "  %s%ld    %s\n", spaces, file_stat.st_size, de->d_name);
                total_len += file_stat.st_size;
            }
        }

        httpd_resp_sendstr_chunk(req, buff);
        fclose(fp);
    }

    closedir(dir);

    sprintf(buff, "\nUsed %d\tbytes\nFree %d\tbytes\n", total_len, get_fs_free_space());
    httpd_resp_sendstr_chunk(req, buff);

    httpd_resp_sendstr_chunk(req, NULL);

    return ESP_OK;
}


static httpd_handle_t webserver_start(void) {

    httpd_handle_t server = NULL;
    esp_err_t ret = ESP_FAIL;

    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    http_config.stack_size = 8096;
    http_config.uri_match_fn = httpd_uri_match_wildcard;
//    http_config.max_uri_handlers = 16;

    ESP_LOGI(TAG, "Starting webserver");


    if (httpd_start(&server, &http_config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        ret = httpd_register_uri_handler(server, &upload_html);
        if (ret != ESP_OK) ESP_LOGE(TAG, "URL \"%s\" not registered. (%s:%u)", upload_html.uri, __FILE__, __LINE__);
        ret = httpd_register_uri_handler(server, &list_html);
        if (ret != ESP_OK) ESP_LOGE(TAG, "URL \"%s\" not registered. (%s:%u)", list_html.uri, __FILE__, __LINE__);
        ret = httpd_register_uri_handler(server, &uri_html);
        if (ret != ESP_OK) ESP_LOGE(TAG, "URL \"%s\" not registered. (%s:%u)", uri_html.uri, __FILE__, __LINE__);
        return server;
    }

    ESP_LOGE(TAG, "Error starting server! (%s:%u)", __FILE__, __LINE__);
    return NULL;
}

static void webserver_stop(httpd_handle_t server) {
    // Stop the httpd server
    httpd_stop(server);
}

static void webserver_disconnect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    httpd_handle_t *server = (httpd_handle_t*) arg;
//    if (*server && !staApModeNow) {
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        webserver_stop(*server);
        *server = NULL;
    }
}

static void webserver_connect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    httpd_handle_t *server = (httpd_handle_t*) arg;
    if (*server == NULL) {
//        ESP_LOGI(TAG, "Starting webserver");
        *server = webserver_start();
    }
}


void webserver_init(const char *html_path) {

    static httpd_handle_t server = NULL;

    ESP_LOGI(TAG, "Initializing webserver.");

    webserver_html_path = malloc(strlen(html_path) + 1);

    if (webserver_html_path == NULL) {
        ESP_LOGE(TAG, "Error allocation memory. (%s:%u)", __FILE__, __LINE__);
        return;
    }

    strcpy(webserver_html_path, html_path);

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &webserver_connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &webserver_connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &webserver_disconnect_handler, &server));

}
