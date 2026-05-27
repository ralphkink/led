#include "asb_utils.h"
#include "../asb_config.h"

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <mbedtls/md.h>
#include <mbedtls/base64.h>
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
            snprintf(buf, sizeof(buf), "%%%02x", (unsigned char)c);
            result += buf;
        }
    }
    return result;
}

// Generates a SAS token for the given resource URI.
// Requires system time to be set (configTime / NTP) for a valid expiry.
static String buildSasToken(const String& resourceUri) {
    // Print system time for debugging — if this is wrong, the generated token will be rejected by ASB.
    time_t now = time(nullptr);
    Serial.printf("Current time: %s", ctime(&now));
    Serial.flush();

    // Decode the Base64 SAS key to raw HMAC key bytes
    const size_t keyBufLen = ((strlen(ASB_KEY) + 3) / 4) * 3 + 1;
    uint8_t* keyBuf = new uint8_t[keyBufLen];
    size_t keyLen = 0;
    mbedtls_base64_decode(keyBuf, keyBufLen, &keyLen,
                          (const uint8_t*)ASB_KEY, strlen(ASB_KEY));

    time_t expiry = time(nullptr) + 3600;

    String encodedUri = urlEncode(resourceUri);
    String toSign = encodedUri + "\n" + String((unsigned long)expiry);

    Serial.printf("Key bytes decoded: %u\n", (unsigned)keyLen);
    Serial.printf("Encoded URI: %s\n", encodedUri.c_str());
    Serial.printf("String to sign: %s\n", toSign.c_str());
    Serial.flush();

    // HMAC-SHA256 of toSign using the raw key
    uint8_t hmac[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, keyBuf, keyLen);
    mbedtls_md_hmac_update(&ctx, (const uint8_t*)toSign.c_str(), toSign.length());
    mbedtls_md_hmac_finish(&ctx, hmac);
    mbedtls_md_free(&ctx);
    delete[] keyBuf;

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
    String url = resourceUri + "/messages/head?timeout=0";
    String token = buildSasToken(resourceUri);

    WiFiClientSecure client;
    // Certificate validation is skipped — acceptable on a constrained embedded target
    // where loading a CA bundle would consume significant flash/RAM.
    client.setInsecure();

    HTTPClient http;
    if (!http.begin(client, url)) return nullptr;

    http.addHeader("Authorization", token);

    // DELETE = receive-and-delete mode: message is removed from the queue on first read.
    // timeout=0 means the server returns immediately (204 No Content) if the queue is empty.
    int code = http.sendRequest("DELETE");
    Serial.printf("ASB HTTP response code: %d\n", code);
    Serial.flush();
    if (code != 200) {
        http.end();
        return nullptr;
    }

    String payload = http.getString();
    http.end();

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
