// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt

#include "esp8266.h"

#include "uart.h"
#include "crc16.h"
#include "serbridge.h"
#include "serled.h"
#include "config.h"
#include "console.h"

static struct espconn serbridgeConn; // plain bridging port
static esp_tcp serbridgeTcp;

// Connection pool
serbridgeConnData connData[MAX_CONN];

// Receive callback
static void ICACHE_FLASH_ATTR
serbridgeRecvCb(void *arg, char *data, unsigned short len)
{
}

// Send all data in conn->txbuffer
// returns result from espconn_sent if data in buffer or ESPCONN_OK (0)
// Use only internally from espbuffsend and serbridgeSentCb
static sint8 ICACHE_FLASH_ATTR
sendtxbuffer(serbridgeConnData *conn)
{
  sint8 result = ESPCONN_OK;
  if (conn->txbufferlen != 0) {
    //os_printf("TX %p %d\n", conn, conn->txbufferlen);
    conn->readytosend = false;
    result = espconn_sent(conn->conn, (uint8_t*)conn->txbuffer, conn->txbufferlen);
    conn->txbufferlen = 0;
    if (result != ESPCONN_OK) {
      os_printf("sendtxbuffer: espconn_sent error %d on conn %p\n", result, conn);
      conn->txbufferlen = 0;
      if (!conn->txoverflow_at) conn->txoverflow_at = system_get_time();
    } else {
      conn->sentbuffer = conn->txbuffer;
      conn->txbuffer = NULL;
      conn->txbufferlen = 0;
    }
  }
  return result;
}

// espbuffsend adds data to the send buffer. If the previous send was completed it calls
// sendtxbuffer and espconn_sent.
// Returns ESPCONN_OK (0) for success, -128 if buffer is full or error from  espconn_sent
// Use espbuffsend instead of espconn_sent as it solves the problem that espconn_sent must
// only be called *after* receiving an espconn_sent_callback for the previous packet.
static sint8 ICACHE_FLASH_ATTR
espbuffsend(serbridgeConnData *conn, const char *data, uint16 len)
{
  if (conn->txbufferlen >= MAX_TXBUFFER) goto overflow;

  // make sure we indeed have a buffer
  if (conn->txbuffer == NULL) conn->txbuffer = os_zalloc(MAX_TXBUFFER);
  if (conn->txbuffer == NULL) {
    os_printf("espbuffsend: cannot alloc tx buffer\n");
    return -128;
  }

  // add to send buffer
  uint16_t avail = conn->txbufferlen+len > MAX_TXBUFFER ? MAX_TXBUFFER-conn->txbufferlen : len;
  os_memcpy(conn->txbuffer + conn->txbufferlen, data, avail);
  conn->txbufferlen += avail;

  // try to send
  sint8 result = ESPCONN_OK;
  if (conn->readytosend) result = sendtxbuffer(conn);

  if (avail < len) {
    // some data didn't fit into the buffer
    if (conn->txbufferlen == 0) {
      // we sent the prior buffer, so try again
      return espbuffsend(conn, data+avail, len-avail);
    }
    goto overflow;
  }
  return result;

overflow:
  if (conn->txoverflow_at) {
    // we've already been overflowing
    if (system_get_time() - conn->txoverflow_at > 10*1000*1000) {
      // no progress in 10 seconds, kill the connection
      os_printf("serbridge: killing overlowing stuck conn %p\n", conn);
      espconn_disconnect(conn->conn);
    }
    // else be silent, we already printed an error
  } else {
    // print 1-time message and take timestamp
    os_printf("serbridge: txbuffer full, conn %p\n", conn);
    conn->txoverflow_at = system_get_time();
  }
  return -128;
}

//callback after the data are sent
static void ICACHE_FLASH_ATTR
serbridgeSentCb(void *arg)
{
  serbridgeConnData *conn = ((struct espconn*)arg)->reverse;
  //os_printf("Sent CB %p\n", conn);
  if (conn == NULL) return;
  //os_printf("%d ST\n", system_get_time());
  if (conn->sentbuffer != NULL) os_free(conn->sentbuffer);
  conn->sentbuffer = NULL;
  conn->readytosend = true;
  conn->txoverflow_at = 0;
  sendtxbuffer(conn); // send possible new data in txbuffer
}

// callback with a buffer of characters that have arrived on the uart
void ICACHE_FLASH_ATTR
serbridgeUartCb(char *buf, short length)
{
  // push buffer into web-console
  for (short i=0; i<length; i++)
    console_write_char(buf[i]);
  // push the buffer into each open connection
  for (short i=0; i<MAX_CONN; i++) {
    if (connData[i].conn) {
      espbuffsend(&connData[i], buf, length);
    }
  }

  serledFlash(50); // short blink on serial LED
}

//===== Connect / disconnect

// Disconnection callback
static void ICACHE_FLASH_ATTR
serbridgeDisconCb(void *arg)
{
  serbridgeConnData *conn = ((struct espconn*)arg)->reverse;
  if (conn == NULL) return;
  // Free buffers
  if (conn->sentbuffer != NULL) os_free(conn->sentbuffer);
  conn->sentbuffer = NULL;
  if (conn->txbuffer != NULL) os_free(conn->txbuffer);
  conn->txbuffer = NULL;
  conn->txbufferlen = 0;
  // Send reset to attached uC if it was in programming mode
  conn->conn = NULL;
}

// Connection reset callback (note that there will be no DisconCb)
static void ICACHE_FLASH_ATTR
serbridgeResetCb(void *arg, sint8 err)
{
  os_printf("serbridge: connection reset err=%d\n", err);
  serbridgeDisconCb(arg);
}

// New connection callback, use one of the connection descriptors, if we have one left.
static void ICACHE_FLASH_ATTR
serbridgeConnectCb(void *arg)
{
  struct espconn *conn = arg;
  // Find empty conndata in pool
  int i;
  for (i=0; i<MAX_CONN; i++) if (connData[i].conn==NULL) break;
#ifdef SERBR_DBG
  os_printf("Accept port %d, conn=%p, pool slot %d\n", conn->proto.tcp->local_port, conn, i);
#endif
  if (i==MAX_CONN) {
#ifdef SERBR_DBG
    os_printf("Aiee, conn pool overflow!\n");
#endif
    espconn_disconnect(conn);
    return;
  }

  os_memset(connData+i, 0, sizeof(struct serbridgeConnData));
  connData[i].conn = conn;
  conn->reverse = connData+i;
  connData[i].readytosend = true;

  espconn_regist_recvcb(conn, serbridgeRecvCb);
  espconn_regist_disconcb(conn, serbridgeDisconCb);
  espconn_regist_reconcb(conn, serbridgeResetCb);
  espconn_regist_sentcb(conn, serbridgeSentCb);

  espconn_set_opt(conn, ESPCONN_REUSEADDR|ESPCONN_NODELAY);
}

//===== Initialization

static void ICACHE_FLASH_ATTR
serbridgeInitPins()
{
#ifdef SERBR_DBG
  os_printf("Serbridge pins: swap=%d\n",
      flashConfig.swap_uart);
#endif

  if (flashConfig.swap_uart) {
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, 4); // RX
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, 4); // TX
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_MTDO_U);
    if (flashConfig.rx_pullup) PIN_PULLUP_EN(PERIPHS_IO_MUX_MTCK_U);
    else                       PIN_PULLUP_DIS(PERIPHS_IO_MUX_MTCK_U);
    system_uart_swap();
  } else {
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, 0);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, 0);
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
    if (flashConfig.rx_pullup) PIN_PULLUP_EN(PERIPHS_IO_MUX_U0RXD_U);
    else                       PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0RXD_U);
    system_uart_de_swap();
  }
}

// Start transparent serial bridge TCP server on specified port (typ. 2323)
void ICACHE_FLASH_ATTR
serbridgeInit(int port1)
{
  serbridgeInitPins();

  os_memset(connData, 0, sizeof(connData));
  os_memset(&serbridgeTcp, 0, sizeof(serbridgeTcp));

  // set-up the primary port for plain bridging
  serbridgeConn.type = ESPCONN_TCP;
  serbridgeConn.state = ESPCONN_NONE;
  serbridgeTcp.local_port = port1;
  serbridgeConn.proto.tcp = &serbridgeTcp;

  espconn_regist_connectcb(&serbridgeConn, serbridgeConnectCb);
  espconn_accept(&serbridgeConn);
  espconn_tcp_set_max_con_allow(&serbridgeConn, MAX_CONN);
  espconn_regist_time(&serbridgeConn, SER_BRIDGE_TIMEOUT, 0);
}
