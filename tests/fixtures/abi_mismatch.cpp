#include "ogw/plugin_abi.h"

namespace {

struct AbiPlugin {
    ogw_plugin base;
};

int32_t abi_get_info(ogw_plugin*, ogw_plugin_info* info) {
    info->id = "abi-mismatch";
    info->name = "ABI Mismatch";
    info->version = "0";
    info->abi_version = 999;
    return OGW_OK;
}

int32_t abi_noop(ogw_plugin*, char*, size_t) { return OGW_ERR_ABI; }

ogw_plugin_vtable abi_vtable{sizeof(ogw_plugin_vtable), abi_get_info, abi_noop, abi_noop, abi_noop, nullptr, nullptr};

}  // namespace

extern "C" {

__attribute__((visibility("default"))) ogw_plugin* ogw_plugin_create(const ogw_host_api*, const char*) {
    auto* plugin = new AbiPlugin;
    plugin->base.vtable = &abi_vtable;
    return &plugin->base;
}

__attribute__((visibility("default"))) void ogw_plugin_destroy(ogw_plugin* plugin) {
    delete reinterpret_cast<AbiPlugin*>(plugin);
}

}
