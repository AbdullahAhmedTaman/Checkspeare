#pragma once

#include <filesystem>
#include <string>

// Folder containing the running executable. Empty path if it can't be determined.
// On macOS inside a .app bundle this is Contents/MacOS.
const std::filesystem::path& executableDir();

// Folder containing the app's assets (computed once, cached).
//   Non-production builds : the source assets folder (RESOURCES_PATH).
//   Production, macOS     : <bundle>/Contents/Resources/assets
//   Production, other     : <executable folder>/assets
const std::filesystem::path& resourcesDir();

// UTF-8 std::string for APIs that take `const char*` file names (e.g. ImGui).
// Prefer passing std::filesystem::path directly whenever the API accepts it.
inline std::string toUtf8(const std::filesystem::path& p)
{
    const auto u8 = p.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}