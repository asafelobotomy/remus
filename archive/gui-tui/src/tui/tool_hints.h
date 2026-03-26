#pragma once

#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Lookup table for external tool installation hints.
 *
 * When a screen needs an external tool (chdman, xdelta3, flips, etc.)
 * and it's not found, this provides user-friendly install instructions.
 */
namespace ToolHints {

struct ToolInfo {
    std::string name;        ///< Display name
    std::string binary;      ///< Binary to check (via which/command -v)
    std::string installHint; ///< Install command / instructions
    std::string description; ///< What it does
};

inline const std::vector<ToolInfo> &allTools()
{
    static const std::vector<ToolInfo> tools = {
        {"chdman",   "chdman",   "sudo apt install mame-tools  (Debian/Ubuntu)\nDNF: sudo dnf install mame-tools\nArch: sudo pacman -S mame-tools",
         "MAME CHD Compressed Hunks of Data converter"},
        {"xdelta3",  "xdelta3",  "sudo apt install xdelta3  (Debian/Ubuntu)\nDNF: sudo dnf install xdelta3\nArch: sudo pacman -S xdelta3",
         "Delta/patch engine for xdelta format (.xdelta, .vcdiff)"},
        {"flips",    "flips",    "Build from source: https://github.com/Alcaro/Flips\nOr install via AUR: yay -S flips",
         "Floating IPS / BPS patcher"},
        {"7z",       "7z",       "sudo apt install p7zip-full  (Debian/Ubuntu)\nDNF: sudo dnf install p7zip-plugins\nArch: sudo pacman -S p7zip",
         "7-Zip archive creation and extraction"},
        {"unzip",    "unzip",    "sudo apt install unzip  (Debian/Ubuntu)\nDNF: sudo dnf install unzip",
         "ZIP archive extraction"},
        {"maxcso",   "maxcso",   "sudo apt install maxcso  (Debian/Ubuntu)\nBuild from source: https://github.com/unknownbrackets/maxcso",
         "CSO/ZSO compressed ISO tool for PSP"},
    };
    return tools;
}

/// Check if a binary is available on PATH.
inline bool isToolAvailable(const std::string &binary)
{
    std::string cmd = "command -v " + binary + " >/dev/null 2>&1";
    return system(cmd.c_str()) == 0;
}

/// Get the install hint for a specific binary, or empty if not found in registry.
inline std::string getInstallHint(const std::string &binary)
{
    for (const auto &t : allTools()) {
        if (t.binary == binary)
            return t.installHint;
    }
    return {};
}

/// Get all missing tools from the registry.
inline std::vector<const ToolInfo *> getMissingTools()
{
    std::vector<const ToolInfo *> missing;
    for (const auto &t : allTools()) {
        if (!isToolAvailable(t.binary))
            missing.push_back(&t);
    }
    return missing;
}

} // namespace ToolHints
