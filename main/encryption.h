#ifndef ENCRYPTION_H
#define ENCRYPTION_H

// Initialization functions for DTLS server
void InitDTLSServer(void);

// Initialization functions for DTLS client
void InitDTLSClient(void);

// Function for receiving decrypted packets
void ReceiveDecryptedPacket(void);

// Function for sending encrypted packets
void SendEncryptedPacket(const char *data, uint16_t length, const char *dest_ip, const char *dest_port);

#endif