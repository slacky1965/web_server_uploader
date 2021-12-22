#ifndef MAIN_INCLUDE_UTILS_H_
#define MAIN_INCLUDE_UTILS_H_

#define MOUNT_POINT_SPIFFS  "/spiffs"
#define DELIM               "/"
#define DELIM_CHR           '/'
#define MAX_BUFF_RW         2048

bool get_status_spiffs();
size_t get_fs_free_space();
void init_spiffs();

#endif /* MAIN_INCLUDE_UTILS_H_ */
