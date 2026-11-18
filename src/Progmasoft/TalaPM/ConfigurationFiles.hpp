// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "Progmasoft/TalaPM/Configuration.hpp"
#include "Progmasoft/TalaPM/Error.hpp"

namespace Progmasoft::TalaPM
{

    /// The largest configuration file TalaPM reads, 1 MiB; a larger one is
    /// refused rather than read into memory.
    constexpr std::uintmax_t kMaxConfigurationBytes = 1024 * 1024;

    /// Reads the configuration sources of a system:
    /// `etc/talapm.conf.d/config.toml`, `etc/talapm.conf.toml`, and every file
    /// under `etc/talapm.d/`, at any depth.
    ///
    /// A missing `config.toml` or `talapm.conf.toml` reads as empty, and a
    /// missing `talapm.d/` as holding nothing. Under `talapm.d/`, symbolic
    /// links to files are read; links to directories are not followed, so a
    /// link cannot make the walk loop. Which files count, and in which order,
    /// is OrderDropIns' choice.
    ///
    /// @param root     The root of the system, `/` on TalaOS.
    /// @param settings The `-s` settings of the command line, in their order.
    /// @return The sources, or kBadConfiguration naming a file that cannot be
    ///         read or is larger than kMaxConfigurationBytes.
    Expected<ConfigurationSources>
    ReadConfigurationSources(const std::filesystem::path &root,
                             std::vector<std::string> settings);

    /// Reads the configuration sources of a system and applies them.
    ///
    /// @param root     The root of the system.
    /// @param settings The `-s` settings.
    /// @return The configuration, or kBadConfiguration.
    Expected<Configuration>
    LoadSystemConfiguration(const std::filesystem::path &root,
                            std::vector<std::string> settings);

} // namespace Progmasoft::TalaPM
