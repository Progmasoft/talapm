// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
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

    /// The defaults with the `-s` settings on top, reading no file, as
    /// `--default-config` asks.
    ///
    /// @param settings The `-s` settings.
    /// @return The configuration, or kBadConfiguration.
    Expected<Configuration>
    LoadDefaultConfiguration(std::vector<std::string> settings);

    /// The keyring of a repository of the user,
    /// `etc/talapm.keyrings/repo/<name>.gpg` under the root; none for a
    /// built-in repository, whose keyring is TalaPM's own.
    ///
    /// @param root       The root of the system.
    /// @param repository The repository.
    /// @return The path, or nothing for a built-in repository.
    std::optional<std::filesystem::path>
    KeyringOf(const std::filesystem::path &root,
              const RepositorySettings &repository);

    /// What `config validate` checks beyond reading the configuration: that
    /// every enabled repository of the user has its keyring.
    ///
    /// @param root          The root of the system.
    /// @param configuration The configuration, read.
    /// @return Nothing, or kBadConfiguration naming the first missing keyring.
    Expected<void>
    ValidateSystem(const std::filesystem::path &root,
                   const Configuration &configuration);

} // namespace Progmasoft::TalaPM
