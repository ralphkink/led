// Azure Service Bus credentials — fill in your values.
// This file is gitignored; never commit real keys.
#pragma once

#define ASB_HOST     "mayerlkink.servicebus.windows.net"
#define ASB_QUEUE    "picture"
#define ASB_KEY_NAME "RootManageSharedAccessKey"
// Base64-encoded primary key from the Azure portal SAS policy page
//#define ASB_KEY      "your-base64-encoded-sas-key=="
#define ASB_KEY      "bXlTZWNyZXRLZXk9PQ=="  // dummy key for testing; won't work with real ASB requests
