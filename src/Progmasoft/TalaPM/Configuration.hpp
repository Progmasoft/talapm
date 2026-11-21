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
        /// How much TalaPM writes, such as "User-Friendly".
        std::string verbosity = "User-Friendly";
    };

    /// A repository of `[repo.built-in.<name>]`.
    struct RepositorySettings
    {
        std::string name;
        bool isEnabled = false;
        /// Its mirrors, first tried first; each is an https:// address.
        std::vector<std::string> mirrorUris;
    };

    /// The configuration of TalaPM, once every source is applied.
    struct Configuration
    {
        DownloadSettings download;
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

    /// The drop-ins that count, in the order they are read: files ending in
    /// `.toml` (so `.conf.toml` too, but not `.toml.conf`), by the bytes of the
    /// name, then of the path. A later one overrides an earlier one, so
    /// `90-b.toml` is above `10-a.toml`.
    ///
    /// @param dropIns The files under `etc/talapm.d/`.
    /// @return Those that count, the lowest first.
    std::vector<ConfigurationFile>
    OrderDropIns(std::vector<ConfigurationFile> dropIns);

    /// Applies every source, from the lowest to the highest, to the defaults of
    /// `etc/talapm.conf.toml`.
    ///
    /// @param sources The sources.
    /// @return The configuration, or kBadConfiguration naming the source.
    Expected<Configuration>
    LoadConfiguration(const ConfigurationSources &sources);

} // namespace Progmasoft::TalaPM
