// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <cstddef>
#include <functional>
#include <utility>

#include "Progmasoft/TalaPM/CommandLine.hpp"

namespace Progmasoft::TalaPM
{
    namespace
    {

        using Commands::CacheScope;
        using Commands::ListKind;
        using Commands::RepositoryFilter;

        /// The arguments once the settings are taken out: options and operands,
        /// each in its order.
        struct Words
        {
            std::vector<std::string> options;
            std::vector<std::string> operands;
            std::vector<std::string> settings;
            bool hasHelp = false;
        };

        tl::unexpected<Error>
        Usage(std::string message)
        {
            return Fail(ErrorCode::kUsage, std::move(message));
        }

        /// Takes the value of `-s` or `--setting` at an argument, attached or
        /// next.
        ///
        /// @return How many arguments it used, or an error.
        Expected<std::size_t>
        TakeSetting(const std::vector<std::string_view> &arguments,
                    std::size_t index,
                    Words &words)
        {
            std::string_view argument = arguments[index];
            constexpr std::string_view kLong = "--setting=";
            if (argument.compare(0, kLong.size(), kLong) == 0)
            {
                words.settings.emplace_back(argument.substr(kLong.size()));
                return 1;
            }
            if (argument.size() > 2 && argument.compare(0, 2, "-s") == 0)
            {
                words.settings.emplace_back(argument.substr(2));
                return 1;
            }
            if (index + 1 >= arguments.size())
            {
                return Usage(std::string(argument) + " needs a setting");
            }
            words.settings.emplace_back(arguments[index + 1]);
            return 2;
        }

        /// Whether an argument is an option or `--`, not an operand.
        bool
        IsOptionWord(std::string_view argument)
        {
            return argument.size() > 1 && argument.front() == '-';
        }

        bool
        IsSetting(std::string_view argument)
        {
            return argument == "--setting"
                   || argument.compare(0, 10, "--setting=") == 0
                   || argument.compare(0, 2, "-s") == 0;
        }

        Expected<Words>
        Split(const std::vector<std::string_view> &arguments)
        {
            Words words;
            bool hasOptionsEnded = false;
            for (std::size_t index = 0; index < arguments.size();)
            {
                std::string_view argument = arguments[index];
                if (!hasOptionsEnded && argument == "--")
                {
                    hasOptionsEnded = true;
                    ++index;
                    continue;
                }
                bool isOption = !hasOptionsEnded && argument.size() > 1
                                && argument.front() == '-';
                if (isOption && IsSetting(argument))
                {
                    Expected<std::size_t> used
                        = TakeSetting(arguments, index, words);
                    if (!used)
                    {
                        return tl::unexpected(used.error());
                    }
                    index += *used;
                    continue;
                }
                words.hasHelp
                    |= isOption && (argument == "--help" || argument == "-h");
                // A short option takes what follows its letter as its value:
                // -uU is -u with the value U, as -s'...' is -s with its
                // setting.
                bool hasValue
                    = isOption && argument.size() > 2 && argument[1] != '-';
                if (hasValue)
                {
                    return Usage(std::string(argument.substr(0, 2))
                                 + " takes no value, but was given '"
                                 + std::string(argument.substr(2)) + "'");
                }
                // An option that takes no value is not followed by a word,
                // which would read as its value: `install pkg -U` or `install
                // -U -- pkg`.
                bool isFollowedByWord = isOption && index + 1 < arguments.size()
                                        && !IsOptionWord(arguments[index + 1]);
                if (isFollowedByWord)
                {
                    return Usage(
                        std::string(argument)
                        + " takes no value, but was "
                          "given '"
                        + std::string(arguments[index + 1])
                        + "'; put the operands before it, or after --");
                }
                (isOption ? words.options : words.operands)
                    .emplace_back(argument);
                ++index;
            }
            return words;
        }

        /// An option a command takes, and what it sets.
        struct Option
        {
            std::string_view name;
            std::string_view shortName;
            std::function<void()> set;
        };

        /// Applies the options of a command, refusing any it does not take.
        Expected<void>
        ApplyOptions(std::string_view command,
                     const std::vector<std::string> &options,
                     const std::vector<Option> &known)
        {
            for (const std::string &given : options)
            {
                const Option *match = nullptr;
                for (const Option &option : known)
                {
                    bool isShort = !option.shortName.empty()
                                   && given == option.shortName;
                    match = given == option.name || isShort ? &option : match;
                }
                if (match == nullptr)
                {
                    return Usage(std::string(command) + " has no option "
                                 + given);
                }
                match->set();
            }
            return {};
        }

        /// Checks the number of operands after the command words.
        Expected<void>
        ExpectOperands(std::string_view command,
                       const std::vector<std::string> &operands,
                       std::size_t least,
                       std::size_t most)
        {
            if (operands.size() < least)
            {
                return Usage(std::string(command) + " needs "
                             + (least == 1 ? "a package" : "more operands"));
            }
            if (operands.size() > most)
            {
                return Usage(std::string(command) + " takes no operand '"
                             + operands[most] + "'");
            }
            return {};
        }

        /// Builds a command from its options and its operands, once the command
        /// words are taken off.
        using Builder = std::function<Expected<Command>(
            const std::vector<std::string> &,
            const std::vector<std::string> &)>;

        template<typename Value>
        Expected<Command>
        Finish(Expected<void> result, Value value)
        {
            if (!result)
            {
                return tl::unexpected(result.error());
            }
            return Command(std::move(value));
        }

        Expected<Command>
        BuildInstall(const std::vector<std::string> &options,
                     const std::vector<std::string> &operands)
        {
            Commands::Install install{ operands };
            Expected<void> result
                = ApplyOptions("install",
                               options,
                               { { "--update",
                                   "-u",
                                   [&] {
                                       install.shouldUpdate = true;
                                   } },
                                 { "--upgrade",
                                   "-U",
                                   [&] {
                                       install.shouldUpgrade = true;
                                   } },
                                 { "--needs",
                                   "",
                                   [&] {
                                       install.isNeedsOnly = true;
                                   } },
                                 { "--with-recommendeds", "", [&] {
                                      install.withRecommended = true;
                                  } } });
            if (result)
            {
                result
                    = ExpectOperands("install", operands, 1, operands.size());
            }
            return Finish(std::move(result), std::move(install));
        }

        Expected<Command>
        BuildRemove(std::string_view name,
                    const std::vector<std::string> &options,
                    const std::vector<std::string> &operands)
        {
            Commands::Remove remove{ operands, name == "purge" };
            Expected<void> result = ApplyOptions(name, options, {});
            if (result)
            {
                result = ExpectOperands(name, operands, 1, operands.size());
            }
            return Finish(std::move(result), std::move(remove));
        }

        Expected<Command>
        BuildUpgrade(std::string_view name,
                     const std::vector<std::string> &options,
                     const std::vector<std::string> &operands)
        {
            Commands::Upgrade upgrade{ operands };
            upgrade.isFull = name == "full-upgrade";
            Expected<void> result
                = ApplyOptions(name,
                               options,
                               { { "--update",
                                   "-u",
                                   [&] {
                                       upgrade.shouldUpdate = true;
                                   } },
                                 { "--check", "", [&] {
                                      upgrade.isCheckOnly = true;
                                  } } });
            if (result)
            {
                std::size_t most = upgrade.isFull ? 0 : operands.size();
                result = ExpectOperands(name, operands, 0, most);
            }
            return Finish(std::move(result), std::move(upgrade));
        }

        Expected<Command>
        BuildList(const std::vector<std::string> &options,
                  const std::vector<std::string> &operands)
        {
            Commands::List list;
            auto choose = [&](ListKind kind) {
                if (std::find(list.kinds.begin(), list.kinds.end(), kind)
                    == list.kinds.end())
                {
                    list.kinds.push_back(kind);
                }
            };
            Expected<void> result
                = ApplyOptions("list",
                               options,
                               { { "--installed",
                                   "",
                                   [&] {
                                       choose(ListKind::kInstalled);
                                   } },
                                 { "--available",
                                   "",
                                   [&] {
                                       choose(ListKind::kAvailable);
                                   } },
                                 { "--upgradable",
                                   "",
                                   [&] {
                                       choose(ListKind::kUpgradable);
                                   } },
                                 { "--versions",
                                   "",
                                   [&] {
                                       choose(ListKind::kVersions);
                                   } },
                                 { "--depends",
                                   "",
                                   [&] {
                                       choose(ListKind::kDepends);
                                   } },
                                 { "--rdepends", "", [&] {
                                      choose(ListKind::kReverseDepends);
                                  } } });
            bool needsPackage
                = std::any_of(list.kinds.begin(),
                              list.kinds.end(),
                              [](ListKind kind) {
                                  return kind == ListKind::kVersions
                                         || kind == ListKind::kDepends
                                         || kind == ListKind::kReverseDepends;
                              });
            if (result && list.kinds.empty())
            {
                result = Usage(
                    "list needs --installed, --available, --upgradable, "
                    "--versions, --depends or --rdepends");
            }
            if (result)
            {
                result
                    = ExpectOperands("list", operands, needsPackage ? 1 : 0, 1);
            }
            if (result && !operands.empty())
            {
                list.package = operands[0];
            }
            return Finish(std::move(result), std::move(list));
        }

        /// Builds a command of one operand and no option.
        template<typename Value>
        Builder
        OneOperand(std::string_view name, std::string Value::*field)
        {
            return [name, field](const std::vector<std::string> &options,
                                 const std::vector<std::string> &operands) {
                Value value{};
                Expected<void> result = ApplyOptions(name, options, {});
                if (result)
                {
                    result = ExpectOperands(name, operands, 1, 1);
                }
                if (result)
                {
                    value.*field = operands[0];
                }
                return Finish(std::move(result), std::move(value));
            };
        }

        /// Builds a command of no operand and no option.
        template<typename Value>
        Builder
        Bare(std::string_view name)
        {
            return [name](const std::vector<std::string> &options,
                          const std::vector<std::string> &operands) {
                Expected<void> result = ApplyOptions(name, options, {});
                if (result)
                {
                    result = ExpectOperands(name, operands, 0, 0);
                }
                return Finish(std::move(result), Value{});
            };
        }

        Expected<Command>
        BuildUpdate(const std::vector<std::string> &options,
                    const std::vector<std::string> &operands)
        {
            Commands::Update update;
            Expected<void> result
                = ApplyOptions("update", options, { { "--upgrade", "-U", [&] {
                                                         update.shouldUpgrade
                                                             = true;
                                                     } } });
            if (result)
            {
                result = ExpectOperands("update", operands, 0, 0);
            }
            return Finish(std::move(result), update);
        }

        Expected<Command>
        BuildCache(const std::vector<std::string> &options,
                   const std::vector<std::string> &operands)
        {
            std::string_view action = operands.empty()
                                          ? std::string_view()
                                          : std::string_view(operands[0]);
            std::vector<std::string> rest(operands.begin()
                                              + (operands.empty() ? 0 : 1),
                                          operands.end());
            if (action == "make" || action == "make!")
            {
                Commands::CacheMake make{ action == "make!" };
                Expected<void> result = ApplyOptions("cache make",
                                                     options,
                                                     { { "--force", "", [&] {
                                                            make.isForced
                                                                = true;
                                                        } } });
                if (result)
                {
                    result = ExpectOperands("cache make", rest, 0, 0);
                }
                return Finish(std::move(result), make);
            }
            if (action != "clean")
            {
                return Usage("cache takes clean or make");
            }
            constexpr std::pair<std::string_view, CacheScope> kScopes[] = {
                { "all", CacheScope::kAll },
                { "packages", CacheScope::kPackages },
                { "meta", CacheScope::kMeta },
            };
            Expected<void> result = ApplyOptions("cache clean", options, {});
            if (result)
            {
                result = ExpectOperands("cache clean", rest, 1, 1);
            }
            for (const auto &[name, scope] : kScopes)
            {
                if (result && rest[0] == name)
                {
                    return Command(Commands::CacheClean{ scope });
                }
            }
            return result ? Usage("cache clean takes all, packages or meta")
                          : tl::unexpected(result.error());
        }

        Expected<Command>
        BuildRepository(const std::vector<std::string> &options,
                        const std::vector<std::string> &operands)
        {
            if (operands.empty() || operands[0] != "list")
            {
                return Usage("repo takes list");
            }
            Commands::RepositoryList list;
            bool isEnabled = false;
            bool isDisabled = false;
            Expected<void> result = ApplyOptions("repo list",
                                                 options,
                                                 { { "--enabled",
                                                     "",
                                                     [&] {
                                                         isEnabled = true;
                                                     } },
                                                   { "--disabled", "", [&] {
                                                        isDisabled = true;
                                                    } } });
            if (isEnabled != isDisabled)
            {
                list.filter = isEnabled ? RepositoryFilter::kEnabled
                                        : RepositoryFilter::kDisabled;
            }
            if (result)
            {
                result
                    = ExpectOperands("repo list",
                                     { operands.begin() + 1, operands.end() },
                                     0,
                                     0);
            }
            return Finish(std::move(result), list);
        }

        /// The builder of each command name.
        Expected<Command>
        Build(std::string_view name,
              const std::vector<std::string> &options,
              const std::vector<std::string> &operands)
        {
            using namespace std::placeholders;
            const std::vector<std::pair<std::string_view, Builder>> kBuilders{
                { "install", BuildInstall },
                { "remove", std::bind(BuildRemove, "remove", _1, _2) },
                { "purge", std::bind(BuildRemove, "purge", _1, _2) },
                { "update", BuildUpdate },
                { "upgrade", std::bind(BuildUpgrade, "upgrade", _1, _2) },
                { "full-upgrade",
                  std::bind(BuildUpgrade, "full-upgrade", _1, _2) },
                { "auto-remove", Bare<Commands::AutoRemove>("auto-remove") },
                { "search", OneOperand("search", &Commands::Search::text) },
                { "info", OneOperand("info", &Commands::Info::package) },
                { "list", BuildList },
                { "policy", OneOperand("policy", &Commands::Policy::package) },
                { "provides",
                  OneOperand("provides", &Commands::Provides::path) },
                { "cache", BuildCache },
                { "repo", BuildRepository },
                { "explain", OneOperand("explain", &Commands::Explain::code) },
            };
            for (const auto &[known, build] : kBuilders)
            {
                if (name == known)
                {
                    return build(options, operands);
                }
            }
            return Usage("no command '" + std::string(name) + "'");
        }

    } // namespace

    Expected<Invocation>
    ParseCommandLine(const std::vector<std::string_view> &arguments)
    {
        Expected<Words> words = Split(arguments);
        if (!words)
        {
            return tl::unexpected(words.error());
        }
        if (words->hasHelp || words->operands.empty())
        {
            if (!words->hasHelp && !words->options.empty())
            {
                return Usage("no command before " + words->options[0]);
            }
            return Invocation{ Commands::Help{}, std::move(words->settings) };
        }
        std::string name = words->operands[0];
        std::vector<std::string> operands(words->operands.begin() + 1,
                                          words->operands.end());
        Expected<Command> command = Build(name, words->options, operands);
        if (!command)
        {
            return tl::unexpected(command.error());
        }
        return Invocation{ std::move(*command), std::move(words->settings) };
    }

} // namespace Progmasoft::TalaPM
