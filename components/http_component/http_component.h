#ifndef HTTP_COMPONENT_H
#define HTTP_COMPONENT_H

#include <esp_http_server.h>

httpd_handle_t start_webserver(void);
void stop_webserver(httpd_handle_t server);

#endif /* HTTP_COMPONENT_H */