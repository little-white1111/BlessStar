#include "bs/db/mgmt/db_plugin_loader.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <filesystem>

using namespace bs::db::mgmt;

bool DbPluginLoader::LoadOne(const std::string& plugin_path, DbDriverFactory& factory)
{
#ifdef _WIN32
    HMODULE h = LoadLibraryA(plugin_path.c_str());
    if (!h) return false;
    auto init = reinterpret_cast<void(*)(DbDriverFactory&)>(
        GetProcAddress(h, "bs_driver_init"));
    if (!init) { FreeLibrary(h); return false; }
    init(factory);
    return true;
#else
    void* h = dlopen(plugin_path.c_str(), RTLD_NOW);
    if (!h) return false;
    auto init = reinterpret_cast<void(*)(DbDriverFactory&)>(
        dlsym(h, "bs_driver_init"));
    if (!init) { dlclose(h); return false; }
    init(factory);
    return true;
#endif
}

int DbPluginLoader::ScanAndLoad(const std::string& dir, DbDriverFactory& factory)
{
    int count = 0;
    if (!std::filesystem::exists(dir)) return 0;
    for (auto& entry : std::filesystem::directory_iterator(dir))
    {
        auto ext = entry.path().extension().string();
#ifdef _WIN32
        if (ext == ".dll")
#else
        if (ext == ".so")
#endif
        {
            if (LoadOne(entry.path().string(), factory))
                ++count;
        }
    }
    return count;
}
