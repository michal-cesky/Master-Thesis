#include <string.h>
#include <stdio.h>
#include "esp_log.h"

#include "mbedtls/ssl.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h" 
#include "mbedtls/timing.h"
#include "mbedtls/debug.h"
#include "mbedtls/platform.h"
#include "mbedtls/ssl_cookie.h"
#include "mbedtls/x509.h"

#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"

#include "configuration.h"
#include "encryption.h"

static const char *Secure_TAG = "ENCRYPTION";

mbedtls_net_context listen_fd;
mbedtls_net_context client_fd;
mbedtls_net_context server_fd;
mbedtls_ssl_context ssl;
mbedtls_ssl_config conf;
mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;

mbedtls_ssl_cookie_ctx cookie_ctx;
mbedtls_x509_crt srvcert;
mbedtls_x509_crt cacert;
mbedtls_pk_context pkey;

mbedtls_timing_delay_context timer;

static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
unsigned char buffer[1500];
int error_code;
int handshake_done = 0;
unsigned char client_ip[16] = { 0 };
size_t cliip_len;

typedef struct {
    uint32_t int_ms;
    uint32_t fin_ms;
    TickType_t start;
} dtls_timer_context;

// Declaration of server and CA certificates and private key
const char server_cert[];
const char ca_cert[];
const char server_key[];

// Function for debugging DTLS
void DTLSDebug(void *ctx, int level, const char *file, int line, const char *str);

// Function for initializing FAT filesystem
//void FATCertsInit(void);




void InitDTLSServer(void) {
    ESP_LOGI(Secure_TAG, "Initializing DTLS server...");

  //  FATCertsInit();

    mbedtls_net_init(&listen_fd);
    mbedtls_net_init(&client_fd);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    mbedtls_ssl_cookie_init(&cookie_ctx);
    mbedtls_x509_crt_init(&srvcert);
    mbedtls_pk_init(&pkey);

    mbedtls_debug_set_threshold(4);
    mbedtls_ssl_conf_dbg(&conf, DTLSDebug, stdout);


    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        ESP_LOGE(Secure_TAG, "Failed to initialize PSA Crypto implementation: %d", (int)status);
        error_code = MBEDTLS_ERR_SSL_HW_ACCEL_FAILED;
        return;
    }

    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

    if (mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)SERVER_PERS, strlen(SERVER_PERS)) != 0) {
        ESP_LOGE(Secure_TAG, "Failed to initialize random number generator");
        return;
    }

    error_code = mbedtls_x509_crt_parse(&srvcert, (const unsigned char *) server_cert, strlen(server_cert) + 1);
    //error_code = mbedtls_x509_crt_parse_file(&srvcert, "/certs/srvcrt.pem");
    if (error_code != 0) {
        ESP_LOGE(Secure_TAG, "Failed to parse server certificate with error: %x", (unsigned int) error_code);
        return;
    }

    error_code = mbedtls_x509_crt_parse(&srvcert, (const unsigned char *) ca_cert, strlen(ca_cert) + 1);
    //error_code = mbedtls_x509_crt_parse_file(&srvcert, "/certs/cacrt.pem");
    if (error_code != 0) {
        ESP_LOGE(Secure_TAG, "Failed to parse CA certificate with error: %d", error_code);
        return;
    }

    error_code = mbedtls_pk_parse_key(&pkey, (const unsigned char *)server_key, strlen(server_key) + 1, NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg);
    //error_code = mbedtls_pk_parse_keyfile(&pkey, "/certs/srvkey.pem", NULL, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (error_code != 0) {
        ESP_LOGE(Secure_TAG, "Failed to parse server private key with error: %d", error_code);
        return;
    }

    error_code = mbedtls_net_bind(&listen_fd, TARGET_IP, TARGET_PORT, MBEDTLS_NET_PROTO_UDP); 
    if (error_code != 0) {
        ESP_LOGE(Secure_TAG, "Failed to bind UDP socket with error: %d", error_code);
        return;
    }

    error_code = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_DATAGRAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if(error_code != 0) {   
        ESP_LOGE(Secure_TAG, "Failed to set SSL configuration with error: %d", error_code);
        return;
    }

    mbedtls_ssl_conf_read_timeout(&conf, READ_TIMEOUT_MS);

    mbedtls_ssl_conf_ca_chain(&conf, srvcert.next, NULL);

    error_code = mbedtls_ssl_conf_own_cert(&conf, &srvcert, &pkey);
    if(error_code != 0) {
        ESP_LOGE(Secure_TAG, "Failed to configurate own certificate with error: %d", error_code);
        return;
    }

    error_code = mbedtls_ssl_cookie_setup(&cookie_ctx, mbedtls_ctr_drbg_random, &ctr_drbg);
    if(error_code != 0) {
        ESP_LOGE(Secure_TAG, "Fail to setup server cookies with error: %d", error_code);
        return;
    }

    mbedtls_ssl_conf_dtls_cookies(&conf, mbedtls_ssl_cookie_write, mbedtls_ssl_cookie_check, &cookie_ctx);

    error_code = mbedtls_ssl_setup(&ssl, &conf);
    if(error_code != 0) {
        ESP_LOGE(Secure_TAG, "Failed to setup SSL with error: %d", error_code);
        return;
    }

    mbedtls_ssl_set_timer_cb(&ssl, &timer, mbedtls_timing_set_delay, mbedtls_timing_get_delay);
 
    ESP_LOGI(Secure_TAG, "DTLS server initialized successfully");
}

void InitDTLSClient(void) {
    ESP_LOGI(Secure_TAG, "Initializing DTLS client...");

  //  FATCertsInit();

    mbedtls_net_init(&server_fd);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);    

    mbedtls_x509_crt_init(&cacert);

    mbedtls_ssl_conf_dbg(&conf, DTLSDebug, stdout);
    mbedtls_debug_set_threshold(0);


    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        ESP_LOGE(Secure_TAG, "Failed to initialize PSA Crypto implementation: %d", (int)status);
        error_code = MBEDTLS_ERR_SSL_HW_ACCEL_FAILED;
        return;
    }

    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

    if (mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)CLIENT_PERS, strlen(CLIENT_PERS)) != 0) {
        ESP_LOGE(Secure_TAG, "Failed to initialize random number generator");
        return;
    }

    error_code = mbedtls_x509_crt_parse(&cacert, (const unsigned char *) ca_cert, strlen(ca_cert) + 1);
    //error_code = mbedtls_x509_crt_parse_file(&cacert, "/certs/cacrt.pem");
    if (error_code != 0) {
        ESP_LOGE(Secure_TAG, "Failed to parse CA certificate with error: %d", error_code);
        return;
    }

    error_code = mbedtls_net_connect(&server_fd, TARGET_IP, TARGET_PORT, MBEDTLS_NET_PROTO_UDP);
    if (error_code != 0) {
        ESP_LOGE(Secure_TAG, "Failed to connect to the server with error: %d", error_code);
        return;
    } else {
        ESP_LOGI(Secure_TAG, "Successfully connected to server %s:%s", TARGET_IP, TARGET_PORT);
    }

    error_code = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_DATAGRAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (error_code != 0) {
        ESP_LOGE(Secure_TAG, "Failed to set SSL configuration with error: %d", error_code);
        return;
    }

    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);

    mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);

    mbedtls_ssl_conf_read_timeout(&conf, READ_TIMEOUT_MS);

    error_code = mbedtls_ssl_setup(&ssl, &conf);
    if(error_code != 0) {
        ESP_LOGE(Secure_TAG, "Failed to setup SSL with error: %d", error_code);
        return;
    }

    error_code = mbedtls_ssl_set_hostname(&ssl, SERVER_NAME);
    if(error_code != 0) {
        ESP_LOGE(Secure_TAG, "Failed to setup a hostname with error: %d", error_code);
        return;
    }

    mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);

    mbedtls_ssl_set_timer_cb(&ssl, &timer, mbedtls_timing_set_delay, mbedtls_timing_get_delay);

    ESP_LOGI(Secure_TAG, "DTLS client initialized successfully");
}



void ReceiveDecryptedPacket(void) {
    int lenght;

    mbedtls_net_free(&client_fd);
    mbedtls_ssl_session_reset(&ssl);

    error_code = mbedtls_net_accept(&listen_fd, &client_fd, client_ip, sizeof(client_ip), &cliip_len);
    if(error_code != 0) {
        ESP_LOGE(Secure_TAG, "Failed to waiting fo r socket with error: %d", error_code);
    }

    error_code = mbedtls_ssl_set_client_transport_id(&ssl, client_ip, cliip_len);
    if(error_code != 0) {
        ESP_LOGE(Secure_TAG, "Failed to set HelloVerifyRequest cookies with error: %d", error_code);
    }

    mbedtls_ssl_set_bio(&ssl, &client_fd, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);

    while (1) {  // While for receiving packets   
        if (!handshake_done) {
            ESP_LOGI(Secure_TAG, "Starting handshake...");
    
            do {
                error_code = mbedtls_ssl_handshake(&ssl);
            } while (error_code == MBEDTLS_ERR_SSL_WANT_READ || error_code == MBEDTLS_ERR_SSL_WANT_WRITE);
    
            if (error_code == MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED) {
                ESP_LOGW(Secure_TAG, "Handshake failed: HelloVerifyRequest required. Restarting handshake...");

                mbedtls_net_free(&client_fd);
                mbedtls_ssl_session_reset(&ssl);

                error_code = mbedtls_net_accept(&listen_fd, &client_fd, client_ip, sizeof(client_ip), &cliip_len);
                if(error_code != 0) {
                    ESP_LOGE(Secure_TAG, "Failed to waiting fo r socket with error: %d", error_code);
                }
                    
                error_code = mbedtls_ssl_set_client_transport_id(&ssl, client_ip, cliip_len);
                if(error_code != 0) {
                    ESP_LOGE(Secure_TAG, "Failed to set HelloVerifyRequest cookies with error: %d", error_code);
                }
                continue;  // Restart handshake

            } else if (error_code != 0) {
                ESP_LOGE(Secure_TAG, "Handshake failed with error %d. Restarting handshake...", error_code);

                mbedtls_net_free(&client_fd);
                mbedtls_ssl_session_reset(&ssl);
                
                error_code = mbedtls_net_accept(&listen_fd, &client_fd, client_ip, sizeof(client_ip), &cliip_len);
                if(error_code != 0) {
                    ESP_LOGE(Secure_TAG, "Failed to waiting fo r socket with error: %d", error_code);
                }
            
                error_code = mbedtls_ssl_set_client_transport_id(&ssl, client_ip, cliip_len);
                if(error_code != 0) {
                    ESP_LOGE(Secure_TAG, "Failed to set HelloVerifyRequest cookies with error: %d", error_code);
                }
                continue;  // Restart handshake
            }
    
            ESP_LOGI(Secure_TAG, "DTLS handshake successful, waiting for data...");
            handshake_done = 1;
        }

        // Receive encrypted packet
        do {
            lenght = mbedtls_ssl_read(&ssl, (unsigned char *)buffer, sizeof(buffer) - 1);
        } while (lenght == MBEDTLS_ERR_SSL_WANT_READ || lenght == MBEDTLS_ERR_SSL_WANT_WRITE);
    
        if (lenght < 0) {
            ESP_LOGE(Secure_TAG, "Failed to receive encrypted packet");
            continue;  // Try to receive data again
        } else {
            buffer[lenght] = '\0';
            ESP_LOGI(Secure_TAG, "Decrypted packet: %s\n", buffer);
        }
    }

    mbedtls_ssl_close_notify(&ssl);
    mbedtls_net_free(&listen_fd);
}

void SendEncryptedPacket(const char *data, uint16_t length, const char *dest_ip, const char *dest_port) {
    if (dest_ip == NULL || strlen(dest_ip) == 0) {
        ESP_LOGE(Secure_TAG, "Invalid destination IP address");
        return;
    }

    ESP_LOGI(Secure_TAG, "Connecting to %s:%s", dest_ip, dest_port);
    ESP_LOGI(Secure_TAG, "Starting handshake...");

    while (1) {
        if (!handshake_done) {

            do {
                error_code = mbedtls_ssl_handshake(&ssl);
            } while (error_code == MBEDTLS_ERR_SSL_WANT_READ || error_code == MBEDTLS_ERR_SSL_WANT_WRITE);

            if (error_code != 0) {
                ESP_LOGE(Secure_TAG, "Handshake failed with error: %d", error_code);
                continue; // Try handshake again
            }

            ESP_LOGI(Secure_TAG, "Handshake successful");
            handshake_done = 1;
        }

        while(handshake_done) {
            ESP_LOGI(Secure_TAG, "Try to send encrypted packet");
            do {
                error_code = mbedtls_ssl_write(&ssl, (unsigned char *) MESSAGE, length);
            } while (error_code == MBEDTLS_ERR_SSL_WANT_READ || error_code == MBEDTLS_ERR_SSL_WANT_WRITE);
        
            if (error_code < 0) {
                ESP_LOGE(Secure_TAG, "Failed to send packet with error: %d", error_code);
                continue;
            }
            
            ESP_LOGI(Secure_TAG, "Packet sent successfully\n");
            vTaskDelay(pdMS_TO_TICKS(TIMER_FOR_SEND_MESSAGE));
        }

        mbedtls_ssl_close_notify(&ssl);
        mbedtls_net_free(&server_fd);
    }
}



void DTLSDebug(void *ctx, int level, const char *file, int line, const char *str) {
    ((void) level);
    fprintf((FILE *) ctx, "%s:%04d: %s", file, line, str);
    fflush((FILE *) ctx);
}

void FATCertsInit(void) {
    esp_vfs_fat_mount_config_t mount_config  = {
        .max_files = 3,
        .format_if_mount_failed = false
    };

    wl_handle_t s_wl_handle;
    error_code = esp_vfs_fat_spiflash_mount_ro("/certs", "certs", &mount_config);
    if (error_code != ESP_OK) {
        ESP_LOGE("CERTS", "Failed to mount certs partition with error: %d\n", error_code);
    }
}


// Server certificate
const char server_cert[] =  "-----BEGIN CERTIFICATE-----\n"
                            "MIIC6jCCAdKgAwIBAgIUJ0xvTMqI7ahDp3xorO4MW3sgezcwDQYJKoZIhvcNAQEL\n"
                            "BQAwDTELMAkGA1UEAwwCQ0EwHhcNMjUwNTAxMTUzMjQ3WhcNMzUwNDI5MTUzMjQ3\n"
                            "WjANMQswCQYDVQQDDAJDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n"
                            "AMMmGarpdPWso86pxdbX6eMo9IP/6PgKdzlHhclhb6wcN0GKYi33+3fUiJhVUGDj\n"
                            "RJJp8zADwLtvUPs7H23cbYO1ZDZHQVtAM13u3azVOO0ODbcJFYkmwncUZ6nBoeJu\n"
                            "/MRQT9/25sEUhMonFul65eEfyuz/vvDT/3vUx9c/07H8zlz8pM+EJeBvItdHwATy\n"
                            "Qz87I2aFRPLFm2+Gyl8PEWK3yEwWH1pHWEGKykzwrJ3/bt2mvsCbCqhkdz6diGRX\n"
                            "XIiYVRPCdBa8teqOryGzzV6VprWwMXaipmGj9jGZFixXFDTtLWhiYnEEM2mEcAQz\n"
                            "Z+DCffR3Ek1yMMVpybOwTOkCAwEAAaNCMEAwHQYDVR0OBBYEFCrleyUcaZzwwod0\n"
                            "YlCSWGrWfpibMB8GA1UdIwQYMBaAFP+zYB+3cZqAas5sfzDAt/rlGNZrMA0GCSqG\n"
                            "SIb3DQEBCwUAA4IBAQCh36Mqedc9fl4zVZc4A85MalBYDWgbSwUaWWUcr3VPCzfH\n"
                            "ZQ7LQoUktIqEQyHYaEwjSdjcJ2anVqZYjWWYmHhdSYVPQILt46QftrRlVrrmP4co\n"
                            "o3gIkC1FiHYfi1iPrFbAkO98fLNkVpGSwjNwHuADGRoWkhjWHYkun3LN8ZmeceBZ\n"
                            "vUrpWLFQC9SxrCKrWHHHEue2f+QIynNfyFvzz0bd5JQVLyi3+SV/EpnjZqJoN50R\n"
                            "HIhGGapX/N/XaDtArfpGiGEnNCJxJC92cX1DaMljMNglKbIsE6G1Twgxp5WVyeqQ\n"
                            "Cq2+caX5mC9rEwk/EhgYRufRG7QzqoU89cKKAvKo\n"
                            "-----END CERTIFICATE-----\n";



// Server private key
const char server_key[] = "-----BEGIN PRIVATE KEY-----\n"
                            "MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDDJhmq6XT1rKPO\n"
                            "qcXW1+njKPSD/+j4Cnc5R4XJYW+sHDdBimIt9/t31IiYVVBg40SSafMwA8C7b1D7\n"
                            "Ox9t3G2DtWQ2R0FbQDNd7t2s1TjtDg23CRWJJsJ3FGepwaHibvzEUE/f9ubBFITK\n"
                            "JxbpeuXhH8rs/77w0/971MfXP9Ox/M5c/KTPhCXgbyLXR8AE8kM/OyNmhUTyxZtv\n"
                            "hspfDxFit8hMFh9aR1hBispM8Kyd/27dpr7AmwqoZHc+nYhkV1yImFUTwnQWvLXq\n"
                            "jq8hs81elaa1sDF2oqZho/YxmRYsVxQ07S1oYmJxBDNphHAEM2fgwn30dxJNcjDF\n"
                            "acmzsEzpAgMBAAECggEADNvxXAxHKzsljiQsx7PxkwjrV9lgohuacJlQbQ0xHR+8\n"
                            "2mJChvHszaAhIhyZD9FZ/uXhYvwUUqWKqgtizkv0oXWt9U+rtFYeLGXlkJJ6TlCb\n"
                            "QcDk/OUjclJTZGHAh5m1qT/7i3ALE5UFDQcXYOo6xKNiLUzK6bamgqPqSTpI+lTn\n"
                            "ShoSkvubapDJyF1IzEXHu+TKqg6wCcNicn/3onkFQyJcocz8p4IDWX3CHyoUvZCE\n"
                            "MpQSiie1A1MPFLCH8fSafEXrz1X77BKMnzyqAxnzBqzMRf2VpjJolCGn4BiC78WD\n"
                            "nG/1ZjUqXeRwGKOe+jEupM5znhPtFjfaC6Xlo5KbQwKBgQDsCqOkOc/ncuIY/1bL\n"
                            "diq02/1+tBAwbnyBidIiIJv/+dqawpHDX7FNaneEZAGNz7sQyMKTb5Vpk3T63f2q\n"
                            "M+4TGqQWEobecbf3fZy4+tLi6/eIhOWnibs+u6hr1RAUyVqwRCdp80+oDh5pZJIy\n"
                            "C1SR0nkUozOxb9m79LThh5nDdwKBgQDTpkvNVYXdBVq/nbl1sgvgNpJo+0qZuLGK\n"
                            "Lh7/TMk3os17ijES4xnyzUmn2EOwTKqFUbndtKgijA3XvhonfUtaoMMv5yHXgEve\n"
                            "PetWSbzG9hxbf5y1COkEy/8n+L6If4/L8ytRztlAeVElRqy6xG2P5hbhPSKWPRHB\n"
                            "wrC9hazKnwKBgBi2AG2387EKUOtHCAIi69OlWEOEfFFlr7ksAYi7GznQIXekOPhY\n"
                            "M0qkg8Cja5o6Dh5ythQTUXQNEOkWhnDkIN91IYqCBAaTpyvMxbSD5cIF7BMpvpUs\n"
                            "kSK/KKGpW5ahgdIEQJAR/dvdJofoFHoSv5bIjw5/C3FfAU9xDeKyfIF/AoGAGyjp\n"
                            "7U2eQgCf5pr846eXcoxOOX6V0igrUEoe3DAkWilgKQxQw9W0zL2fSBSuiv8rmrQk\n"
                            "H9lBAj3qwNa6UHr5ooWixpDofNkP41Ma0hXgTb/jX7J+j5S3IlBzS9PVz3pfH5Ly\n"
                            "6iqbInTu/tOU8tqmHKMwKHNJ7vGjR7QOLiPrSo0CgYEA4W1AzRHgnfUNHRfNm2rJ\n"
                            "qM6RIM/xCHnlw3FlkVLzOY2i/nFtye4YmqJtp0GxbM1WqnA2DzwoLnYiZMXbcpGW\n"
                            "x4EXQkC05U3ESFO56dyZbuDGnQahgvAhQh8fumCAT8cschKUMoEcVazInUmDDJnL\n"
                            "mzUu2XWm6X8qet1YNeGuykY=\n"
                           "-----END PRIVATE KEY-----\n";

// CA certificate
const char ca_cert[] = "-----BEGIN CERTIFICATE-----\n"
                        "MIIC+zCCAeOgAwIBAgIUIUyQG8T9oR/sqjT4xRIpclVUu7kwDQYJKoZIhvcNAQEL\n"
                        "BQAwDTELMAkGA1UEAwwCQ0EwHhcNMjUwNTAxMTUzMjI3WhcNMzUwNDI5MTUzMjI3\n"
                        "WjANMQswCQYDVQQDDAJDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n"
                        "ALvpN69o/7AGUtiek25NUmlCjuBMVshqtllG3zERf3B6ke9D7JN5a2Cw6I5UdICx\n"
                        "Rh3Dq3LNBV5oIWdqOG9TrqnF9QZRIIGJvkHZLwDZw503fIJkhKlJAGRr/uCa0ZqY\n"
                        "Jj0nyARGZEyz5gpS3KXtAN3tsTOihl4AgWvyfvlD9DCodm5CzptgcT5JebLDuSTg\n"
                        "oLLFSVR0MyOc1rNnisNF91h7kenIvvZdMe9g+KXhb1YavEMjybAbo/y5eX8E4s+t\n"
                        "OZH7kz8XkRE1CKCcF7d2yxeiv12RLIe+EM+BixyFI/d4370xckd5ai29iDZlyP0W\n"
                        "F1TPB4ABQogw2Q25Mo2wtxkCAwEAAaNTMFEwHQYDVR0OBBYEFP+zYB+3cZqAas5s\n"
                        "fzDAt/rlGNZrMB8GA1UdIwQYMBaAFP+zYB+3cZqAas5sfzDAt/rlGNZrMA8GA1Ud\n"
                        "EwEB/wQFMAMBAf8wDQYJKoZIhvcNAQELBQADggEBAKGBED5M5lwrTiScA61HkOmo\n"
                        "7B5ULSBhkyEiBWXkJm6Ce90NrSoytM+CJ5MzQLb0fvn089Yly2lLj6aWOO22yTm6\n"
                        "RTvPUWm5p8Rba1R/D8lQyjZwZPodCGoqam+RmUbUorx3WdBROTlG1CIm/e/JpE40\n"
                        "eMmVynNJbROyFUK4XJjRTZdFv9gfmW1FA2/W1/gxzEs17/emhJ5GI59d5V51Jm7V\n"
                        "FKhtc2/41ybjlJZQ5aL2cr1hNjsC0u92Zbt4getrm2ObUJPyhlnhjl3sdk5yjoGu\n"
                        "AKA8qugnb34zJ4IWNwgr0vocbIu6rgtnOyRzE70Gydc3/Z/rgFn4jqvxbux5qlA=\n"
                        "-----END CERTIFICATE-----\n";