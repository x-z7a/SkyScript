
#include "plugin.h"
const char *log_msg_prefix = nullptr;

// =========================== plugin entry points ===============================================
PLUGIN_API int XPluginStart(char *out_name, char *out_sig, char *out_desc)
{
    log_msg_prefix = Manager::instance().getName();
    return Manager::instance().initialize(out_name, out_sig, out_desc);
}

PLUGIN_API int XPluginEnable(void)
{
    Manager::instance().enable();
    return 1;
}

PLUGIN_API void XPluginDisable(void)
{
    Manager::instance().disable();
}