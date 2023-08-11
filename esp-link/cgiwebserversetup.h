#ifndef CGIWEBSERVER_H
#define CGIWEBSERVER_H

#ifdef WEBSERVER
#include <httpd.h>

int ICACHE_FLASH_ATTR cgiWebServerSetupUpload(HttpdConnData *connData);
#endif

#endif /* CGIWEBSERVER_H */
