
#include <esp8266.h>
#include "cgi.h"
#include "espfs.h"
#include "config.h"
#include "serled.h"
#include "status.h"
#include "serbridge.h"

// Cgi to return choice of pin assignments
static int ICACHE_FLASH_ATTR cgiPinsGet(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted

  char buff[1024];
  int len;

  len = os_sprintf(buff,
      "{ \"conn\":%d, \"ser\":%d, \"swap\":%d, \"rxpup\":%d }",
      flashConfig.conn_led_pin, flashConfig.ser_led_pin, !!flashConfig.swap_uart, !!flashConfig.rx_pullup);

  jsonHeader(connData, 200);
  httpdSend(connData, buff, len);
  return HTTPD_CGI_DONE;
}

// Cgi to change choice of pin assignments
static int ICACHE_FLASH_ATTR cgiPinsSet(HttpdConnData *connData) {
  if (connData->conn==NULL) {
    return HTTPD_CGI_DONE; // Connection aborted
  }

  int8_t ok = 0;
  int8_t conn, ser;
  uint8_t swap, rxpup;
  ok |= getInt8Arg(connData, "conn", &conn);
  ok |= getInt8Arg(connData, "ser", &ser);
  ok |= getBoolArg(connData, "swap", &swap);
  ok |= getBoolArg(connData, "rxpup", &rxpup);
  if (ok < 0) return HTTPD_CGI_DONE;

  char *coll;
  if (ok > 0) {
    // check whether two pins collide
    uint16_t pins = 0;
    if (conn >= 0) {
      if (pins & (1<<conn)) { coll = "Conn LED"; goto collision; }
      pins |= 1 << conn;
    }
    if (ser >= 0) {
      if (pins & (1<<ser)) { coll = "Serial LED"; goto collision; }
      pins |= 1 << ser;
    }
    if (swap) {
      if (pins & (1<<15)) { coll = "Uart TX"; goto collision; }
      if (pins & (1<<13)) { coll = "Uart RX"; goto collision; }
    } else {
      if (pins & (1<<1)) { coll = "Uart TX"; goto collision; }
      if (pins & (1<<3)) { coll = "Uart RX"; goto collision; }
    }

    // we're good, set flashconfig
    flashConfig.conn_led_pin = conn;
    flashConfig.ser_led_pin = ser;
    flashConfig.swap_uart = swap;
    flashConfig.rx_pullup = rxpup;
    os_printf("Pins changed: conn=%d ser=%d swap=%d rx-pup=%d\n", 
      conn, ser, swap, rxpup);

    // apply the changes
    serbridgeInitPins();
    serledInit();
    statusInit();

    // save to flash
    if (configSave()) {
      httpdStartResponse(connData, 204);
      httpdEndHeaders(connData);
    } else {
      httpdStartResponse(connData, 500);
      httpdEndHeaders(connData);
      httpdSend(connData, "Failed to save config", -1);
    }
  }
  return HTTPD_CGI_DONE;

collision: {
    char buff[128];
    os_sprintf(buff, "Pin assignment for %s collides with another assignment", coll);
    errorResponse(connData, 400, buff);
    return HTTPD_CGI_DONE;
  }
}

int ICACHE_FLASH_ATTR cgiPins(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  if (connData->requestType == HTTPD_METHOD_GET) {
    return cgiPinsGet(connData);
  } else if (connData->requestType == HTTPD_METHOD_POST) {
    return cgiPinsSet(connData);
  } else {
    jsonHeader(connData, 404);
    return HTTPD_CGI_DONE;
  }
}
