#ifndef WIILINK_CONFIG_H
#define WIILINK_CONFIG_H

#define APP_NAME "WiiLink Patcher Wii"
#define APP_SLUG "wiilink-patcher"
#define APP_VERSION "0.2.0"
#define APP_BUILD "2026-08-13"

#define STORAGE_ROOT "usb:"
#define APP_DIR STORAGE_ROOT "/apps/wiilink-patcher"
#define WORK_DIR STORAGE_ROOT "/wiilink/.tmp"
#define WAD_DIR STORAGE_ROOT "/WAD"
#define LOG_DIR APP_DIR "/logs"
#define DEBUG_LOG_PATH LOG_DIR "/debug.log"
#define CRASH_LOG_PATH LOG_DIR "/crash.log"
#define LANGUAGE_CONFIG_PATH APP_DIR "/language.cfg"

#define NUS_HOST "nus.cdn.shop.wii.com"
#define NUS_BASE "http://nus.cdn.shop.wii.com/ccs/download"
#define PATCHER_HOST "patcher.wiilink24.com"
#define PATCHER_BASE "http://patcher.wiilink24.com"
#define OSC_HOST "hbb1.oscwii.org"
#define OSC_BASE "http://hbb1.oscwii.org"

#define HTTP_CONNECT_TIMEOUT_MS 15000
#define HTTP_IO_TIMEOUT_MS 30000
#define HTTP_RETRIES 3
#define IO_BUFFER_SIZE 0x10000
#define WIILINK_MAX_TMD_SIZE 8192
#define MAX_TICKET_SIZE 4096
#define MAX_CONTENTS 64
#define MAX_MENU_ITEMS 64

#endif
