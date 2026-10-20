#include "ResourcePaths.hpp"

#include <string>
#include <system_error>

#ifndef PRODUCTION_BUILD
#error "PRODUCTION_BUILD must be defined (1 or 0) by the build system"
#endif
#if !PRODUCTION_BUILD && !defined(RESOURCES_PATH)
#error "RESOURCES_PATH must be defined for non-production builds"
#endif

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <cstdint>
#include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;

namespace
{

    fs::path computeExecutableDir()
    {
#if defined(_WIN32)
        std::wstring buf(MAX_PATH, L'\0');
        for (;;)
        {
            const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
            if (n == 0)
                return {};
            if (n < buf.size())
            {
                buf.resize(n);
                break;
            }
            buf.resize(buf.size() * 2);
        }
        return fs::path(buf).parent_path();
#elif defined(__APPLE__)
        std::uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        std::string buf(size, '\0');
        if (_NSGetExecutablePath(buf.data(), &size) != 0)
            return {};
        std::error_code ec;
        return fs::weakly_canonical(fs::path(buf.c_str()), ec).parent_path();
#else
        std::error_code ec;
        return fs::read_symlink("/proc/self/exe", ec).parent_path();
#endif
    }

    fs::path computeResourcesDir()
    {
#if !PRODUCTION_BUILD
        return RESOURCES_PATH; // absolute path to the source assets folder
#elif defined(__APPLE__)
        return (executableDir() / ".." / "Resources" / "assets").lexically_normal();
#else
        return executableDir() / "assets";
#endif
    }

} // namespace

const fs::path& executableDir()
{
    static const fs::path dir = computeExecutableDir();
    return dir;
}

const fs::path& resourcesDir()
{
    static const fs::path dir = computeResourcesDir();
    return dir;
}