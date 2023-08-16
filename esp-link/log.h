#ifndef LOG_H
#define LOG_H

#include "httpd.h"

void logInit(void);
int ajaxLog(HttpdConnData *connData);

#endif
