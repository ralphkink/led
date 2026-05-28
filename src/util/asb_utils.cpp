#include "asb_utils.h"
#include "../asb_config.h"

#include <ArduinoJson.h>
#include <mbedtls/md.h>
#include <mbedtls/base64.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <time.h>

// ---- AsbMessage ----

AsbMessage::AsbMessage(const String& cmd, const String& pic)
    : cmd(cmd), pic(pic) {}

// ---- Internal helpers ----

static String urlEncode(const String& s) {
    String result;
    result.reserve(s.length() * 3);
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            result += c;
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
            result += buf;
        }
    }
    return result;
}

static String decodeChunkedBody(const String& body) {
    String decoded;
    int pos = 0;

    while (pos < body.length()) {
        int lineEnd = body.indexOf("\r\n", pos);
        if (lineEnd < 0) break;

        String sizeText = body.substring(pos, lineEnd);
        int extension = sizeText.indexOf(';');
        if (extension >= 0) sizeText = sizeText.substring(0, extension);

        unsigned long chunkSize = strtoul(sizeText.c_str(), nullptr, 16);
        if (chunkSize == 0) break;

        int dataStart = lineEnd + 2;
        int dataEnd = dataStart + (int)chunkSize;
        if (dataEnd > body.length()) break;

        decoded += body.substring(dataStart, dataEnd);
        pos = dataEnd + 2;
    }

    return decoded;
}

static int httpsDeleteTls12(const String& path, const String& token, String& responseBody) {
    const char* port = "443";
    const char* pers = "asb-tls12";

    mbedtls_net_context serverFd;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context ctrDrbg;
    mbedtls_entropy_context entropy;
    String request;
    String response;
    String headers;
    String body;
    String lowerHeaders;
    const uint8_t* req = nullptr;
    size_t remaining = 0;
    char buf[512];

    mbedtls_net_init(&serverFd);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_ctr_drbg_init(&ctrDrbg);
    mbedtls_entropy_init(&entropy);

    int ret = mbedtls_ctr_drbg_seed(&ctrDrbg, mbedtls_entropy_func, &entropy,
                                    (const unsigned char*)pers, strlen(pers));
    if (ret != 0) {
        Serial.printf("TLS RNG seed failed: -0x%04X\n", -ret);
        goto cleanup;
    }

    ret = mbedtls_net_connect(&serverFd, ASB_HOST, port, MBEDTLS_NET_PROTO_TCP);
    if (ret != 0) {
        Serial.printf("TLS TCP connect failed: -0x%04X\n", -ret);
        goto cleanup;
    }

    ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        Serial.printf("TLS config defaults failed: -0x%04X\n", -ret);
        goto cleanup;
    }

    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctrDrbg);
    mbedtls_ssl_conf_read_timeout(&conf, 10000);
    mbedtls_ssl_conf_min_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_max_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);

    ret = mbedtls_ssl_setup(&ssl, &conf);
    if (ret != 0) {
        Serial.printf("TLS setup failed: -0x%04X\n", -ret);
        goto cleanup;
    }

    ret = mbedtls_ssl_set_hostname(&ssl, ASB_HOST);
    if (ret != 0) {
        Serial.printf("TLS hostname failed: -0x%04X\n", -ret);
        goto cleanup;
    }

    mbedtls_ssl_set_bio(&ssl, &serverFd, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);

    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            Serial.printf("TLS handshake failed: -0x%04X\n", -ret);
            goto cleanup;
        }
    }

    Serial.printf("TLS protocol: %s\n", mbedtls_ssl_get_version(&ssl));
    Serial.flush();

    request = "DELETE " + path + " HTTP/1.1\r\n" +
              "Host: " + String(ASB_HOST) + "\r\n" +
              "Authorization: " + token + "\r\n" +
              "Content-Length: 0\r\n" +
              "Connection: close\r\n\r\n";

    req = (const uint8_t*)request.c_str();
    remaining = request.length();
    while (remaining > 0) {
        ret = mbedtls_ssl_write(&ssl, req, remaining);
        if (ret > 0) {
            req += ret;
            remaining -= ret;
            continue;
        }
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            Serial.printf("TLS write failed: -0x%04X\n", -ret);
            goto cleanup;
        }
    }

    do {
        ret = mbedtls_ssl_read(&ssl, (unsigned char*)buf, sizeof(buf) - 1);
        if (ret > 0) {
            buf[ret] = '\0';
            response += buf;
        }
    } while (ret > 0 || ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    if (ret != 0 && ret != MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
        Serial.printf("TLS read ended with: -0x%04X\n", -ret);
    }

    {
        int firstSpace = response.indexOf(' ');
        int secondSpace = response.indexOf(' ', firstSpace + 1);
        int statusCode = -1;
        if (firstSpace >= 0 && secondSpace > firstSpace) {
            statusCode = response.substring(firstSpace + 1, secondSpace).toInt();
        }

        int headerEnd = response.indexOf("\r\n\r\n");
        headers = headerEnd >= 0 ? response.substring(0, headerEnd) : "";
        body = headerEnd >= 0 ? response.substring(headerEnd + 4) : response;

        lowerHeaders = headers;
        lowerHeaders.toLowerCase();
        responseBody = lowerHeaders.indexOf("transfer-encoding: chunked") >= 0
                       ? decodeChunkedBody(body)
                       : body;

        ret = statusCode;
    }

cleanup:
    mbedtls_ssl_close_notify(&ssl);
    mbedtls_net_free(&serverFd);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctrDrbg);
    mbedtls_entropy_free(&entropy);
    return ret;
}

// Generates a SAS token for the given resource URI.
// Requires system time to be set (configTime / NTP) for a valid expiry.
static String buildSasToken(const String& resourceUri) {
    // Print system time for debugging — if this is wrong, the generated token will be rejected by ASB.
    time_t now = time(nullptr);
    Serial.printf("Current time: %s", ctime(&now));
    Serial.flush();

    // Azure Service Bus SAS keys look like Base64, but the literal key string
    // from the portal is the HMAC key. Do not Base64-decode it.
    const uint8_t* key = (const uint8_t*)ASB_KEY;
    const size_t keyLen = strlen(ASB_KEY);

    time_t expiry = time(nullptr) + 3600;

    String encodedUri = urlEncode(resourceUri);
    String toSign = encodedUri + "\n" + String((unsigned long)expiry);

    Serial.printf("SAS key string length: %u\n", (unsigned)keyLen);
    Serial.printf("Encoded URI: %s\n", encodedUri.c_str());
    Serial.printf("String to sign: %s\n", toSign.c_str());
    Serial.flush();

    // HMAC-SHA256 of toSign using the literal SAS key string
    uint8_t hmac[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, key, keyLen);
    mbedtls_md_hmac_update(&ctx, (const uint8_t*)toSign.c_str(), toSign.length());
    mbedtls_md_hmac_finish(&ctx, hmac);
    mbedtls_md_free(&ctx);
    // Base64-encode then URL-encode the signature
    uint8_t sigBuf[48] = {};
    size_t sigLen = 0;
    mbedtls_base64_encode(sigBuf, sizeof(sigBuf), &sigLen, hmac, sizeof(hmac));
    String sig = urlEncode(String((char*)sigBuf, sigLen));

    String token = "SharedAccessSignature sr=" + encodedUri +
                   "&sig=" + sig +
                   "&se=" + String((unsigned long)expiry) +
                   "&skn=" + String(ASB_KEY_NAME);
    Serial.printf("SAS token: %s\n", token.c_str());
    Serial.flush();
    return token;
}

// ---- Public API ----

AsbMessage* readAsbMessage() {
    // Resource URI is the queue path — used both in the SAS token and the request URL
    String resourceUri = "https://" + String(ASB_HOST) + "/" + String(ASB_QUEUE);
    String path = "/" + String(ASB_QUEUE) + "/messages/head?timeout=0";
    String token = buildSasToken(resourceUri);

    // DELETE = receive-and-delete mode: message is removed from the queue on first read.
    // timeout=0 means the server returns immediately (204 No Content) if the queue is empty.
    String payload;
    int code = httpsDeleteTls12(path, token, payload);
    Serial.printf("ASB HTTP response code: %d\n", code);
    Serial.flush();
    if (code != 200) {
        if (payload.length() > 0) {
            Serial.printf("ASB error response: %s\n", payload.c_str());
            Serial.flush();
        }
        return nullptr;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("Failed to deserialize JSON: %s\n", err.c_str());
        Serial.flush();
        return nullptr;
    }

    const char* cmd = doc["cmd"];
    const char* pic = doc["pic"];
    if (!cmd || !pic) return nullptr;

    return new AsbMessage(cmd, pic);
}
