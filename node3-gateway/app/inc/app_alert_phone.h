#ifndef APP_ALERT_PHONE_H
#define APP_ALERT_PHONE_H

#if __has_include("app_secrets.h")
#include "app_secrets.h"
#else
#warning "app_secrets.h not found: alert calls go to the placeholder number"
#include "app_secrets.example.h"
#endif

#endif
