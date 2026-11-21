// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <utility>

#include "Progmasoft/TalaPM/Configuration.hpp"

// toml++ reports parse errors in its result instead of throwing.
#define TOML_EXCEPTIONS 0
#include <toml.hpp>

#include "Progmasoft/TalaPM/Manifest.hpp"

namespace Progmasoft::TalaPM
{
    namespace
    {

        /// The repositories TalaPM knows, in their order, and whether each is
        /// on.
        constexpr std::pair<std::string_view, bool> kBuiltInRepositories[] = {
            { "core", true },    { "extra", true },        { "lib32", false },
            { "nofree", false }, { "lib32nofree", false },
        };

        tl::unexpected<Error>
        Invalid(std::string_view source, std::string message)
        {
            return Fail(ErrorCode::kBadConfiguration,
                        std::string(source) + ": " + std::move(message));
        }

        /// The file name of a path.
        std::string_view
        BaseName(std::string_view path)
        {
            std::size_t slash = path.rfind('/');
            return slash == std::string_view::npos ? path
                                                   : path.substr(slash + 1);
        }

        bool
        EndsWith(std::string_view text, std::string_view end)
        {
            return text.size() >= end.size()
                   && text.compare(text.size() - end.size(), end.size(), end)
                          == 0;
        }

        Expected<bool>
        ReadBoolean(std::string_view source,
                    const toml::node &node,
                    const char *key)
        {
            std::optional<bool> value = node.value<bool>();
            if (!node.is_boolean() || !value)
            {
                return Invalid(source,
                               std::string(key) + " is not true or false");
            }
            return *value;
        }

        Expected<std::vector<std::string>>
        ReadStrings(std::string_view source,
                    const toml::node &node,
                    const char *key)
        {
            const toml::array *array = node.as_array();
            std::vector<std::string> strings;
            for (std::size_t index = 0;
                 array != nullptr && index < array->size();
                 ++index)
            {
                std::optional<std::string> text
                    = (*array)[index].value<std::string>();
                if (!(*array)[index].is_string() || !text)
                {
                    array = nullptr;
                    break;
                }
                strings.push_back(std::move(*text));
            }
            if (array == nullptr)
            {
                return Invalid(source,
                               std::string(key) + " is not a list of strings");
            }
            return strings;
        }

        Expected<void>
        ApplyDownload(std::string_view source,
                      const toml::table &table,
                      DownloadSettings &download)
        {
            for (const auto &[key, node] : table)
            {
                std::string_view name = key.str();
                if (name == "parallel")
                {
                    std::optional<std::int64_t> count
                        = node.value<std::int64_t>();
                    if (!node.is_integer() || !count || *count < 1
                        || *count > 64)
                    {
                        return Invalid(source,
                                       "download.parallel is not 1 to 64");
                    }
                    download.parallel = static_cast<int>(*count);
                    continue;
                }
                if (name == "verbosity")
                {
                    std::optional<std::string> text = node.value<std::string>();
                    if (!node.is_string() || !text || text->empty())
                    {
                        return Invalid(source,
                                       "download.verbosity is not a word");
                    }
                    download.verbosity = std::move(*text);
                    continue;
                }
                bool *flag = name == "weak-deps"    ? &download.weakDependencies
                             : name == "keep-cache" ? &download.keepCache
                             : name == "check-space" ? &download.checkSpace
                                                     : nullptr;
                if (flag == nullptr)
                {
                    return Invalid(source,
                                   "download has no key " + std::string(name));
                }
                Expected<bool> value = ReadBoolean(source, node, name.data());
                if (!value)
                {
                    return tl::unexpected(value.error());
                }
                *flag = *value;
            }
            return {};
        }

        /// Whether a mirror address is HTTPS, the only transport TalaPM uses.
        bool
        IsHttpsUri(std::string_view uri)
        {
            constexpr std::string_view kScheme = "https://";
            return uri.size() > kScheme.size()
                   && uri.compare(0, kScheme.size(), kScheme) == 0
                   && uri.find_first_of(" \t\r\n") == std::string_view::npos;
        }

        Expected<void>
        ApplyRepository(std::string_view source,
                        std::string_view name,
                        const toml::table &table,
                        RepositorySettings &repository)
        {
            for (const auto &[key, node] : table)
            {
                std::string_view field = key.str();
                if (field == "enabled")
                {
                    Expected<bool> value = ReadBoolean(source, node, "enabled");
                    if (!value)
                    {
                        return tl::unexpected(value.error());
                    }
                    repository.isEnabled = *value;
                    continue;
                }
                if (field != "mirror-uris")
                {
                    return Invalid(source,
                                   "repository " + std::string(name)
                                       + " has no key " + std::string(field));
                }
                Expected<std::vector<std::string>> uris
                    = ReadStrings(source, node, "mirror-uris");
                if (!uris)
                {
                    return tl::unexpected(uris.error());
                }
                for (const std::string &uri : *uris)
                {
                    if (!IsHttpsUri(uri))
                    {
                        return Invalid(source,
                                       "mirror " + uri + " is not https://");
                    }
                }
                repository.mirrorUris = std::move(*uris);
            }
            return {};
        }

        /// The repository tables of `[repo.built-in]`, in the order they are
        /// written.
        Expected<std::vector<std::pair<std::string, const toml::table *>>>
        RepositoryTables(std::string_view source, const toml::node &repo)
        {
            const toml::table *repoTable = repo.as_table();
            const toml::table *builtIn
                = repoTable != nullptr ? (*repoTable)["built-in"].as_table()
                                       : nullptr;
            if (repoTable == nullptr || builtIn == nullptr
                || repoTable->size() != 1)
            {
                return Invalid(source,
                               "repo holds only [repo.built-in.<name>] tables");
            }
            std::vector<std::pair<std::string, const toml::table *>> tables;
            for (const auto &[key, node] : *builtIn)
            {
                if (!node.is_table() || !IsPackageName(key.str()))
                {
                    return Invalid(source,
                                   "repo.built-in." + std::string(key.str())
                                       + " is not a repository table");
                }
                tables.emplace_back(std::string(key.str()), node.as_table());
            }
            // toml++ keeps keys sorted; the order of priority is the written
            // one.
            std::stable_sort(tables.begin(),
                             tables.end(),
                             [](auto &left, auto &right) {
                                 const toml::source_position &a
                                     = left.second->source().begin;
                                 const toml::source_position &b
                                     = right.second->source().begin;
                                 return a.line != b.line ? a.line < b.line
                                                         : a.column < b.column;
                             });
            return tables;
        }

        Expected<void>
        ApplyRepositories(std::string_view source,
                          const toml::node &repo,
                          std::vector<RepositorySettings> &repositories)
        {
            auto tables = RepositoryTables(source, repo);
            if (!tables)
            {
                return tl::unexpected(tables.error());
            }
            for (const auto &entry : *tables)
            {
                const std::string &name = entry.first;
                const toml::table *table = entry.second;
                auto found = std::find_if(repositories.begin(),
                                          repositories.end(),
                                          [&](const RepositorySettings &known) {
                                              return known.name == name;
                                          });
                if (found == repositories.end())
                {
                    repositories.push_back(
                        RepositorySettings{ name, false, {} });
                    found = repositories.end() - 1;
                }
                Expected<void> result
                    = ApplyRepository(source, name, *table, *found);
                if (!result)
                {
                    return result;
                }
            }
            return {};
        }

        Expected<void>
        ApplySignature(std::string_view source,
                       const toml::node &node,
                       Configuration &configuration)
        {
            const toml::table *table = node.as_table();
            if (table == nullptr || table->size() != 1
                || !table->contains("level"))
            {
                return Invalid(source, "signature holds only level");
            }
            Expected<std::vector<std::string>> levels
                = ReadStrings(source, *table->get("level"), "signature.level");
            if (!levels)
            {
                return tl::unexpected(levels.error());
            }
            configuration.signatureLevels = std::move(*levels);
            return {};
        }

        /// Applies one source of TOML text.
        Expected<void>
        Apply(std::string_view source,
              std::string_view text,
              Configuration &configuration)
        {
            toml::parse_result parsed = toml::parse(text, source);
            if (!parsed)
            {
                const toml::parse_error &error = parsed.error();
                return Invalid(source,
                               "line "
                                   + std::to_string(error.source().begin.line)
                                   + ": " + std::string(error.description()));
            }
            const toml::table &table = parsed.table();
            for (const auto &[key, node] : table)
            {
                std::string_view name = key.str();
                Expected<void> result;
                if (name == "download" && node.is_table())
                {
                    result = ApplyDownload(source,
                                           *node.as_table(),
                                           configuration.download);
                }
                else if (name == "signature")
                {
                    result = ApplySignature(source, node, configuration);
                }
                else if (name == "repo")
                {
                    result = ApplyRepositories(source,
                                               node,
                                               configuration.repositories);
                }
                else
                {
                    result = Invalid(source, "no section " + std::string(name));
                }
                if (!result)
                {
                    return result;
                }
            }
            return {};
        }

    } // namespace

    bool
    IsDropInName(std::string_view path)
    {
        std::string_view name = BaseName(path);
        return EndsWith(name, ".toml") && name != ".toml";
    }

    std::vector<ConfigurationFile>
    OrderDropIns(std::vector<ConfigurationFile> dropIns)
    {
        dropIns.erase(std::remove_if(dropIns.begin(),
                                     dropIns.end(),
                                     [](const ConfigurationFile &file) {
                                         return !IsDropInName(file.path);
                                     }),
                      dropIns.end());
        std::stable_sort(
            dropIns.begin(),
            dropIns.end(),
            [](const ConfigurationFile &left, const ConfigurationFile &right) {
                std::string_view a = BaseName(left.path);
                std::string_view b = BaseName(right.path);
                return a != b ? a < b : left.path < right.path;
            });
        return dropIns;
    }

    Expected<Configuration>
    LoadConfiguration(const ConfigurationSources &sources)
    {
        Configuration configuration;
        for (const auto &[name, isEnabled] : kBuiltInRepositories)
        {
            configuration.repositories.push_back(
                RepositorySettings{ std::string(name), isEnabled, {} });
        }
        std::vector<const ConfigurationFile *> lowestFirst{ &sources.base,
                                                            &sources.main };
        // Read in their order, so a later drop-in overrides an earlier one.
        std::vector<ConfigurationFile> dropIns = OrderDropIns(sources.dropIns);
        for (const ConfigurationFile &file : dropIns)
        {
            lowestFirst.push_back(&file);
        }
        for (const ConfigurationFile *file : lowestFirst)
        {
            Expected<void> result
                = Apply(file->path, file->text, configuration);
            if (!result)
            {
                return tl::unexpected(result.error());
            }
        }
        for (std::size_t index = 0; index < sources.settings.size(); ++index)
        {
            Expected<void> result = Apply("-s #" + std::to_string(index + 1),
                                          sources.settings[index],
                                          configuration);
            if (!result)
            {
                return tl::unexpected(result.error());
            }
        }
        return configuration;
    }

} // namespace Progmasoft::TalaPM
