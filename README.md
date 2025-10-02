# Master Thesis — Securing Single-Pair Multidrop Ethernet

This repository contains the implementation and experiments for my master's thesis at Brno University of Technology (VUT v Brně). The project demonstrates reliable, modern, and secure communication in industrial environments using Single Pair Ethernet (SPE) technology with the ESP32 platform.

Thesis link: [http://hdl.handle.net/11012/251484](http://hdl.handle.net/11012/251484)

## Project Overview

This project provides a modular demonstrator for communication over Single Pair Ethernet (SPE) using the ESP32 microcontroller and the Microchip LAN8651 SPE PHY. The firmware allows you to:

- Establish communication between devices over SPE.
- Optionally enable packet sniffing and save network traffic to `.pcap` files.
- Simulate packet injection attacks for security testing.
- Secure the communication using the DTLS protocol.

All features (plain communication, sniffing, attack simulation, encryption) are independent and can be enabled or disabled as needed. You can use only the basic communication, add packet sniffing, or enable DTLS security—each function works separately or together depending on your configuration.

## Components and Dependencies

- **ESP32** — Main microcontroller platform.
- **Microchip LAN8651** — SPE PHY, with official `libtc6` driver component (included, from Microchip).
- **Packet capture component (`espressif__pcap`)** — Used for saving network traffic; this is a third-party component.
- **ESP-IDF 5.3.1** and its standard libraries (lwIP, mbedTLS, SPIFFS, etc.).

## Configuration

All key runtime parameters are set in `main/configuration.h`. You can configure:

- **SPI Settings**:
  - SPI host, SPI mode, frequency, pin assignment for MOSI, MISO, CLK, CS.

- **Network Settings**:
  - `DEVICE_MAC` — MAC address for the device.
  - `DEVICE_IP` — Static IP address for the device.
  - `DEST_IP` — Destination IP address for sending messages.
  - `PORT` — UDP port number.
  - `UDP_MSG` — The message to be sent.

- **PLCA (Physical Layer Collision Avoidance) Settings**:
  - `PLCA_ENABLE` — Enable/disable PLCA.
  - `NODE_ID` — Node ID for PLCA.
  - `NODE_COUNT` — Number of nodes in the network.

- **PHY Transmission Settings**:
  - `BURST_COUNT`, `BURST_TIMER` — Transmission burst settings.
  - `TX_CUT_THROUHG`, `RX_CUT_THROUGH` — Cut-through mode for transmission/reception.

- **Packet Sniffer & Capture**:
  - `SNIFFER` — Enable/disable packet sniffing.
  - `PCAP_FILENAME` — Output file for captured traffic.

- **Security & Attack Simulation**:
  - `ENCRYPTED_SERVER`, `ENCRYPTED_CLIENT` — Enable DTLS server or client mode.
  - `SERVER_NAME`, `SERVER_PERS`, `CLIENT_PERS` — Identifiers for DTLS connections.
  - `READ_TIMEOUT_MS`, `MAX_RETRY` — Timeout and retry settings for secure communication.

- **DTLS Certificates**:
  - Certificates can be stored directly in the code (less secure) or placed in the `certificates` directory.
  - If you use external certificates, uncomment the indicated lines in `main/encryption.c` and `CMakeLists.txt` as described in the code comments.

**For exact setup instructions and more detailed explanations, please refer to the official thesis document:**
[http://hdl.handle.net/11012/251484](http://hdl.handle.net/11012/251484)

## Building and Running

1. **Clone the repository:**
   ```sh
   git clone https://github.com/michal-cesky/Master-Thesis.git
   cd Master-Thesis
   ```

2. **Set up the ESP-IDF v5.3.1 environment** (see [ESP-IDF documentation](https://docs.espressif.com/projects/esp-idf/en/v5.3.1/esp32/get-started/)).

3. **Configure the project:**
   - Adjust `main/configuration.h` to match your hardware and scenario.
   - For DTLS with external certificates, place files in `certificates` and update the code as described above.

4. **Build and flash the firmware:**
   ```sh
   idf.py build
   idf.py -p (YOUR_PORT) flash
   ```

5. **Monitor device output and experiment as needed:**
   ```sh
   idf.py monitor
   ```

---

If you have questions or encounter issues, please open an Issue on GitHub or refer to the thesis for detailed methodology and instructions.
