#include "plugin_local.hpp"

rack::plugin::Plugin* pluginInstance;

void init(rack::plugin::Plugin* p) {
    pluginInstance = p;

    // Register our Workshop Computer module
    p->addModel(modelWorkshopComputer);
}
