#include "esp_log.h"
#include "esp_spiffs.h"

#include "utils.h"

static const char *TAG = "web_server_utils";

static bool spiffs;

bool get_status_spiffs() {
    return spiffs;
}

static esp_vfs_spiffs_conf_t conf = {
        .base_path = MOUNT_POINT_SPIFFS,
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false };

void init_spiffs() {

    ESP_LOGI(TAG, "Initialize SPIFFS");

    spiffs = true;

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NO_MEM) {
            ESP_LOGE(TAG, "Objects could not be allocated. (%s:%u)", __FILE__, __LINE__);
        } else if (ret == ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Already mounted or partition is encrypted. (%s:%u)", __FILE__, __LINE__);
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Partition for SPIFFS was not found. (%s:%u)", __FILE__, __LINE__);
        } else if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Mount or format fails. (%s:%u)", __FILE__, __LINE__);
        }
        spiffs = false;
    }
}

size_t get_fs_free_space() {
    size_t full;
    size_t used;

    if (esp_spiffs_info(conf.partition_label, &full, &used) != ESP_OK) {
        return 0;
    }

    return full - used;
}


