#ifndef ENCRYPTION_H
#define ENCRYPTION_H

#include <stdint.h>
#include <stddef.h>

#include "mbedtls/ssl.h"

extern mbedtls_ssl_context ssl;

void InitEncryption(void);
void SendEncryptedPacket(const char *data, uint16_t length, const char *dest_ip, uint16_t dest_port);
void ReceiveDecryptedPacket(void);
void InitDTLSClient(void);
void InitDTLSServer(void);

extern const char server_cert[];
extern const char ca_cert[];
extern const char server_key[];

#endif