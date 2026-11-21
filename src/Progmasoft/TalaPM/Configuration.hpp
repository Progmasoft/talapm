// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "Progmasoft/TalaPM/Error.hpp"

namespace Progmasoft::TalaPM
{

    /// The `[download]` settings.
    struct DownloadSettings
    {
        /// How many downloads run at once, 1 to 64.
        int parallel = 4;
        /// Whether recommended packages are installed with what needs them.
        bool weakDependencies = false;
        /// Whether downloaded packages stay in the cache after installing.
        bool keepCache = true;
        /// Whether the free space is checked before installing.
        bool checkSpace = true;
        /// How much TalaPM writes, one of kVerbosities.
        std::string verbosity = "User-Friendly";
    };

    /// The values of `download.verbosity`, the most written first.
    /// `--verbose` is "Always-Verbose" and `--quiet` "Always-Silent".
    constexpr std::string_view kVerbosities[] = {
        "Always-Verbose", "Somewhat-Verbose", "User-Friendly",
        "Near-Silent",    "Always-Silent",
    };

    /// The `[color] level`.
    enum class ColorLevel
    {
        kAuto,   ///< Colors when the output is a terminal.
        kAlways, ///< Colors always.
        kNever,  ///< No colors.
    };

    /// A repository: a built-in one, `[repo.built-in.<name>]`, whose mirrors
    /// and keyring are TalaPM's own, or one of the user, `[repo.<name>]`.
    struct RepositorySettings
    {
        std::string name;
        bool isEnabled = false;
        /// Its mirrors, first tried first; each is an https:// address. For a
        /// built-in repository, the mirrors embedded in TalaPM.
        std::vector<std::string> mirrorUris;
        bool isBuiltIn = false;
        /// The source that first named it, for messages about it.
        std::string namedIn;
    };

    /// The mirror embedded in TalaPM for every built-in repository.
    constexpr std::string_view kBuiltInMirror = "https://package.tala.run/";

    /// The configuration of TalaPM, once every source is applied.
    struct Configuration
    {
        DownloadSettings download;
        ColorLevel color = ColorLevel::kAuto;
        /// The `[signature] level` values, such as "Recommend" and "Database".
        std::vector<std::string> signatureLevels{ "Recommend", "Database" };
        /// The repositories in their priority, first first: as written, the
        /// first source to name one placing it.
        std::vector<RepositorySettings> repositories;
    };

    /// A configuration file and its text.
    struct ConfigurationFile
    {
        /// Its path under `etc/`, for messages and for the order of drop-ins.
        std::string path;
        std::string text;
    };

    /// The sources of the configuration, as read from the system.
    struct ConfigurationSources
    {
        /// `etc/talapm.conf.d/config.toml`, the lowest.
        ConfigurationFile base;
        /// `etc/talapm.conf.toml`.
        ConfigurationFile main;
        /// The files under `etc/talapm.d/`, in any order.
        std::vector<ConfigurationFile> dropIns;
        /// The `-s` settings, in the order given; the last is the highest.
        std::vector<std::string> settings;
    };

    /// Whether a file of `etc/talapm.d/` is a drop-in: its name ends in
    /// `.toml`, so `.conf.toml` counts, and `.toml.conf`, `.yaml`, `.json`,
    /// `.ini`, a name without an extension and any other do not.
    ///
    /// @param path The path of the file; only its name is looked at.
    /// @return true when it counts.
    bool
    IsDropInName(std::string_view path);

    /// The drop-ins that count, highest first: files ending in `.toml` (so
    /// `.conf.toml` too, but not `.toml.conf`); those whose name starts with a
    /// character other than a letter or digit, then a digit, then an uppercase
    /// letter, then a lowercase letter, and within each by the bytes of the
    /// name, so `00-a.toml` is above `10-b.toml`.
    ///
    /// @param dropIns The files under `etc/talapm.d/`.
    /// @return Those that count, highest first.
    std::vector<ConfigurationFile>
    OrderDropIns(std::vector<ConfigurationFile> dropIns);

    /// Applies every source, from the lowest to the highest, to the defaults of
    /// `etc/talapm.conf.toml`.
    ///
    /// @param sources The sources.
    /// @return The configuration, or kBadConfiguration naming the source.
    Expected<Configuration>
    LoadConfiguration(const ConfigurationSources &sources);

    /// The configuration of no file: TalaPM's defaults, as installed.
    ///
    /// @return The defaults.
    Configuration
    DefaultConfiguration();

    /// The configuration as TOML, every key in the order of the installed
    /// file and the repositories in their priority; the mirrors of a built-in
    /// repository are TalaPM's own and are not written.
    ///
    /// @param configuration The configuration.
    /// @return The TOML text.
    std::string
    DumpConfiguration(const Configuration &configuration);

    /// One setting, by its dotted key, as a TOML value: `download.parallel` is
    /// `4`, `color.level` is `"Auto"`, `repo.example.mirror-uris` is a list.
    ///
    /// @param configuration The configuration.
    /// @param key           The dotted key.
    /// @return The value, or kUsage for a key that is not set.
    Expected<std::string>
    GetSetting(const Configuration &configuration, std::string_view key);

} // namespace Progmasoft::TalaPM
