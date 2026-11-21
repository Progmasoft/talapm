// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MIT

#include <string>
#include <utility>
#include <vector>

#include "Progmasoft/TalaPM/Configuration.hpp"

namespace Progmasoft::TalaPM
{
    namespace
    {

        /// A TOML basic string: quotes, with `\` and `"` escaped. Settings are
        /// words and https:// addresses, which hold no control characters.
        std::string
        Quoted(std::string_view text)
        {
            std::string quoted = "\"";
            for (char character : text)
            {
                if (character == '"' || character == '\\')
                {
                    quoted += '\\';
                }
                quoted += character;
            }
            return quoted + "\"";
        }

        std::string
        Boolean(bool value)
        {
            return value ? "true" : "false";
        }

        std::string
        Strings(const std::vector<std::string> &values)
        {
            std::string list = "[";
            for (std::size_t index = 0; index < values.size(); ++index)
            {
                list += (index == 0 ? "" : ", ") + Quoted(values[index]);
            }
            return list + "]";
        }

        std::string
        ColorName(ColorLevel level)
        {
            switch (level)
            {
                case ColorLevel::kAlways:
                    return "Always";
                case ColorLevel::kNever:
                    return "Never";
                default:
                    return "Auto";
            }
        }

        /// The table a repository is written as.
        std::string
        TableOf(const RepositorySettings &repository)
        {
            return repository.isBuiltIn ? "repo.built-in." + repository.name
                                        : "repo." + repository.name;
        }

        /// Every key of the configuration and its TOML value, in the order of
        /// the installed file.
        std::vector<std::pair<std::string, std::string>>
        Settings(const Configuration &configuration)
        {
            const DownloadSettings &download = configuration.download;
            std::vector<std::pair<std::string, std::string>> settings{
                { "download.parallel", std::to_string(download.parallel) },
                { "download.weak-deps", Boolean(download.weakDependencies) },
                { "download.keep-cache", Boolean(download.keepCache) },
                { "download.check-space", Boolean(download.checkSpace) },
                { "download.verbosity", Quoted(download.verbosity) },
                { "signature.level", Strings(configuration.signatureLevels) },
                { "color.level", Quoted(ColorName(configuration.color)) },
            };
            for (const RepositorySettings &repository :
                 configuration.repositories)
            {
                settings.emplace_back(TableOf(repository) + ".enabled",
                                      Boolean(repository.isEnabled));
                if (!repository.isBuiltIn)
                {
                    settings.emplace_back(TableOf(repository) + ".mirror-uris",
                                          Strings(repository.mirrorUris));
                }
            }
            return settings;
        }

    } // namespace

    std::string
    DumpConfiguration(const Configuration &configuration)
    {
        std::string text;
        std::string table;
        for (const auto &[key, value] : Settings(configuration))
        {
            std::size_t dot = key.rfind('.');
            std::string owner = key.substr(0, dot);
            if (owner != table)
            {
                text += (text.empty() ? "[" : "\n[") + owner + "]\n";
                table = owner;
            }
            text += key.substr(dot + 1) + " = " + value + "\n";
        }
        return text;
    }

    Expected<std::string>
    GetSetting(const Configuration &configuration, std::string_view key)
    {
        for (const auto &[known, value] : Settings(configuration))
        {
            if (known == key)
            {
                return value;
            }
        }
        return Fail(ErrorCode::kUsage,
                    "no setting " + std::string(key)
                        + "; config dump prints every setting");
    }

} // namespace Progmasoft::TalaPM
