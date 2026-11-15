#ifndef OGW_PLUGIN_ABI_H
#define OGW_PLUGIN_ABI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OGW_PLUGIN_ABI_VERSION 1u

#define OGW_OK 0
#define OGW_ERR_ABI 1
#define OGW_ERR_INIT 2
#define OGW_ERR_STATE 3
#define OGW_ERR_REJECTED 4
#define OGW_ERR_BAD_ARG 5

#define OGW_LOG_ERROR 0
#define OGW_LOG_WARN 1
#define OGW_LOG_INFO 2
#define OGW_LOG_DEBUG 3

#define OGW_VALUE_NUMBER 1
#define OGW_VALUE_BOOL 2
#define OGW_VALUE_STRING 3

#define OGW_QUALITY_GOOD 0
#define OGW_QUALITY_UNCERTAIN 1
#define OGW_QUALITY_BAD 2
#define OGW_QUALITY_STALE 3

typedef struct ogw_reading {
    int32_t kind;
    int32_t quality;
    int32_t bool_value;
    double number;
    const char* string_value;
    const char* detail;
    int64_t source_timestamp_ms;
} ogw_reading;

typedef struct ogw_host_api {
    uint32_t abi_version;
    uint32_t size;
    void* user_data;
    void (*log)(void* user_data, int32_t level, const char* component, const char* message);
    int32_t (*register_device)(void* user_data, const char* json);
    int32_t (*update_device)(void* user_data, const char* json);
    int32_t (*remove_device)(void* user_data, const char* local_id);
    int32_t (*publish_reading)(void* user_data, const char* local_device_id, const char* capability_id,
                               const ogw_reading* reading);
    int64_t (*wall_time_ms)(void* user_data);
} ogw_host_api;

typedef struct ogw_plugin ogw_plugin;

typedef struct ogw_plugin_info {
    const char* id;
    const char* name;
    const char* version;
    uint32_t abi_version;
} ogw_plugin_info;

typedef struct ogw_plugin_vtable {
    uint32_t size;
    int32_t (*get_info)(ogw_plugin* plugin, ogw_plugin_info* info);
    int32_t (*init)(ogw_plugin* plugin, char* err, size_t err_len);
    int32_t (*start)(ogw_plugin* plugin, char* err, size_t err_len);
    int32_t (*stop)(ogw_plugin* plugin, char* err, size_t err_len);
    int32_t (*describe_settings)(ogw_plugin* plugin, char* buf, size_t len);
    int32_t (*invoke)(ogw_plugin* plugin, const char* capability_id, const char* json, char* err, size_t err_len);
} ogw_plugin_vtable;

struct ogw_plugin {
    const ogw_plugin_vtable* vtable;
};

typedef ogw_plugin* (*ogw_plugin_create_fn)(const ogw_host_api* host, const char* config_json);
typedef void (*ogw_plugin_destroy_fn)(ogw_plugin* plugin);

#ifdef __cplusplus
}
#endif

#endif
