#ifndef __SER_BRIDGE_H__
#define __SER_BRIDGE_H__

#include <ip_addr.h>
#include <c_types.h>
#include <espconn.h>

#define MAX_CONN 4
#define SER_BRIDGE_TIMEOUT 300 // 300 seconds = 5 minutes

// Send buffer size
#define MAX_TXBUFFER (2*1460)

typedef struct serbridgeConnData {
	struct espconn *conn;
	uint16         txbufferlen;   // length of data in txbuffer
	char           *txbuffer;     // buffer for the data to send
	char           *sentbuffer;   // buffer sent, awaiting callback to get freed
	uint32_t       txoverflow_at; // when the transmitter started to overflow
	bool           readytosend;   // true, if txbuffer can be sent by espconn_sent
} serbridgeConnData;

void ICACHE_FLASH_ATTR serbridgeInit(int port1);
void ICACHE_FLASH_ATTR serbridgeUartCb(char *buf, short len);

#endif /* __SER_BRIDGE_H__ */
