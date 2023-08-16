#ifndef CONSOLE_H
#define CONSOLE_H

#include "httpd.h"

void ICACHE_FLASH_ATTR console_write_char(char c);
int ajaxConsole(HttpdConnData *connData);

#endif
