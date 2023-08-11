#ifndef CGIMQTT_H
#define CGIMQTT_H

#ifdef MQTT
#include "httpd.h"
int cgiMqtt(HttpdConnData *connData);
#endif // MQTT

char *mqttState(void);

#endif // CGIMQTT_H
