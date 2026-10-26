#pragma once

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef FIREBASE_PROJECT_ID
#define FIREBASE_PROJECT_ID ""
#endif
#ifndef FIREBASE_DATABASE_URL
#define FIREBASE_DATABASE_URL ""
#endif
#ifndef FIREBASE_API_KEY
#define FIREBASE_API_KEY ""
#endif
#ifndef FIREBASE_DEVICE_EMAIL
#define FIREBASE_DEVICE_EMAIL ""
#endif
#ifndef FIREBASE_DEVICE_PASSWORD
#define FIREBASE_DEVICE_PASSWORD ""
#endif
#ifndef FIREBASE_DEVICE_UID
#define FIREBASE_DEVICE_UID ""
#endif
#ifndef FIREBASE_ADMIN_UID
#define FIREBASE_ADMIN_UID ""
#endif
#ifndef FIREBASE_DEVICE_ID
#define FIREBASE_DEVICE_ID ""
#endif

#ifndef DEVICE_FIRMWARE_VERSION
#define DEVICE_FIRMWARE_VERSION "6.7.0-cloud-diagnostics"
#endif
