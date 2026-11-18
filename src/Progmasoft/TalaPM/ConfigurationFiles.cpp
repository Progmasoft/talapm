// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MIT

#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

#include "Progmasoft/TalaPM/ConfigurationFiles.hpp"

namespace Progmasoft::TalaPM
{
    namespace
    {

        tl::unexpected<Error>
        Unreadable(const std::filesystem::path &path, const std::string &reason)
        {
            return Fail(ErrorCode::kBadConfiguration,
                        path.generic_string() + ": " + reason);
        }

        /// The path of a file relative to the root, as messages and the order
        /// of drop-ins name it: `etc/talapm.d/10-mirrors.toml`.
        std::string
        RelativeName(const std::filesystem::path &root,
                     const std::filesystem::path &path)
        {
            std::error_code error;
            std::filesystem::path relative
                = std::filesystem::relative(path, root, error);
            return (error ? path : relative).generic_string();
        }

        /// Reads a file, refusing one larger than kMaxConfigurationBytes.
        Expected<std::string>
        ReadText(const std::filesystem::path &path)
        {
            std::error_code error;
            std::uintmax_t size = std::filesystem::file_size(path, error);
            if (error)
            {
                return Unreadable(path, error.message());
            }
            if (size > kMaxConfigurationBytes)
            {
                return Unreadable(path, "larger than 1 MiB");
            }
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                return Unreadable(path, "cannot be opened");
            }
            std::string text((std::istreambuf_iterator<char>(stream)),
                             std::istreambuf_iterator<char>());
            if (stream.bad())
            {
                return Unreadable(path, "cannot be read");
            }
            return text;
        }

        /// Reads a file that may be missing, as empty when it is.
        Expected<ConfigurationFile>
        ReadOptional(const std::filesystem::path &root, const char *relative)
        {
            std::filesystem::path path = root / relative;
            std::error_code error;
            if (!std::filesystem::exists(path, error))
            {
                if (error)
                {
                    return Unreadable(path, error.message());
                }
                return ConfigurationFile{ relative, "" };
            }
            Expected<std::string> text = ReadText(path);
            if (!text)
            {
                return tl::unexpected(text.error());
            }
            return ConfigurationFile{ relative, std::move(*text) };
        }

        /// Reads every file under `etc/talapm.d/`.
        Expected<std::vector<ConfigurationFile>>
        ReadDropIns(const std::filesystem::path &root)
        {
            std::vector<ConfigurationFile> files;
            std::filesystem::path directory = root / "etc/talapm.d";
            std::error_code error;
            if (!std::filesystem::exists(directory, error))
            {
                return files;
            }
            // Links to directories are not followed, so the walk cannot loop.
            std::filesystem::recursive_directory_iterator entries(
                directory,
                std::filesystem::directory_options::none,
                error);
            for (;
                 !error
                 && entries != std::filesystem::recursive_directory_iterator();
                 entries.increment(error))
            {
                std::error_code kindError;
                if (!entries->is_regular_file(kindError) || kindError)
                {
                    continue;
                }
                Expected<std::string> text = ReadText(entries->path());
                if (!text)
                {
                    return tl::unexpected(text.error());
                }
                files.push_back(
                    ConfigurationFile{ RelativeName(root, entries->path()),
                                       std::move(*text) });
            }
            if (error)
            {
                return Unreadable(directory, error.message());
            }
            return files;
        }

    } // namespace

    Expected<ConfigurationSources>
    ReadConfigurationSources(const std::filesystem::path &root,
                             std::vector<std::string> settings)
    {
        Expected<ConfigurationFile> base
            = ReadOptional(root, "etc/talapm.conf.d/config.toml");
        if (!base)
        {
            return tl::unexpected(base.error());
        }
        Expected<ConfigurationFile> main
            = ReadOptional(root, "etc/talapm.conf.toml");
        if (!main)
        {
            return tl::unexpected(main.error());
        }
        Expected<std::vector<ConfigurationFile>> dropIns = ReadDropIns(root);
        if (!dropIns)
        {
            return tl::unexpected(dropIns.error());
        }
        return ConfigurationSources{ std::move(*base),
                                     std::move(*main),
                                     std::move(*dropIns),
                                     std::move(settings) };
    }

    Expected<Configuration>
    LoadSystemConfiguration(const std::filesystem::path &root,
                            std::vector<std::string> settings)
    {
        Expected<ConfigurationSources> sources
            = ReadConfigurationSources(root, std::move(settings));
        if (!sources)
        {
            return tl::unexpected(sources.error());
        }
        return LoadConfiguration(*sources);
    }

} // namespace Progmasoft::TalaPM
