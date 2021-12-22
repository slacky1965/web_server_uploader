#ifndef MAIN_INCLUDE_HTTP_H_
#define MAIN_INCLUDE_HTTP_H_

#define HTML_PATH MOUNT_POINT_SPIFFS DELIM "html"
#define MODULE_NAME "Robot Car"

void webserver_init(const char *html_path);
void webserver_restart();

#endif /* MAIN_INCLUDE_HTTP_H_ */
