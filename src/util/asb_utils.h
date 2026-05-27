#pragma once

#include <Arduino.h>

// Encapsulates a single message received from Azure Service Bus.
// 'pic' carries a Base64-encoded image string.
class AsbMessage {
public:
    String cmd;
    String pic;

    AsbMessage(const String& cmd, const String& pic);
};

// Non-blocking read from the configured Azure Service Bus queue (see asb_config.h).
// Uses HTTP DELETE with timeout=0 (receive-and-delete mode — message is consumed on receipt).
// Returns a heap-allocated AsbMessage* when a message is available, nullptr otherwise.
// Caller is responsible for deleting the returned pointer.
AsbMessage* readAsbMessage();
