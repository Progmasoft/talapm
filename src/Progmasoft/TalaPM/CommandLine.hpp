// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "Progmasoft/TalaPM/Configuration.hpp"
#include "Progmasoft/TalaPM/Error.hpp"

namespace Progmasoft::TalaPM::Commands
{

    /// `install <package>...`: install, or reinstall what is installed.
    struct Install
    {
        std::vector<std::string> packages;
        /// `--update`, `-u`: synchronize the database first.
        bool shouldUpdate = false;
        /// `--upgrade`, `-U`: upgrade the system packages first.
        bool shouldUpgrade = false;
        /// `--needs`: install only what is not installed.
        bool isNeedsOnly = false;
        /// `--with-recommendeds`: also install the recommended packages.
        bool withRecommended = false;
    };

    /// `remove <package>...`, or with `isPurge`, `purge <package>...`.
    struct Remove
    {
        std::vector<std::string> packages;
        /// Also remove configuration and cache files.
        bool isPurge = false;
    };

    /// `update`: synchronize the database; `--upgrade`, `-U` then upgrades.
    struct Update
    {
        bool shouldUpgrade = false;
    };

    /// `upgrade [<package>...]`, or with `isFull`, `full-upgrade`.
    struct Upgrade
    {
        /// The packages to upgrade; none upgrades the system.
        std::vector<std::string> packages;
        /// `--update`, `-u`: synchronize the database first.
        bool shouldUpdate = false;
        /// `--check`: list what would be upgraded, and for `full-upgrade` what
        /// would be removed, and change nothing.
        bool isCheckOnly = false;
        /// `full-upgrade`: remove packages where the upgrade needs it.
        bool isFull = false;
    };

    /// `auto-remove`: remove the dependencies nothing needs.
    struct AutoRemove
    {};

    /// `search <text>`.
    struct Search
    {
        std::string text;
    };

    /// `info <package>`.
    struct Info
    {
        std::string package;
    };

    /// What `list` lists.
    enum class ListKind
    {
        kInstalled,
        kAvailable,
        kUpgradable,
        kVersions,
        kDepends,
        kReverseDepends,
    };

    /// `list [<package>] --installed --available --upgradable --versions
    /// --depends --rdepends`, any of them together. `--versions`, `--depends`
    /// and `--rdepends` need a package; with a package, the other kinds list
    /// only that package.
    struct List
    {
        /// The kinds asked for, each once, in the order given.
        std::vector<ListKind> kinds;
        /// The package, or empty.
        std::string package;
    };

    /// `policy <package>`: its versions and where they come from.
    struct Policy
    {
        std::string package;
    };

    /// `provides <file>`: the package that installs a file.
    struct Provides
    {
        std::string path;
    };

    /// What `cache clean` removes.
    enum class CacheScope
    {
        kAll,
        kPackages,
        kMeta,
    };

    /// `cache clean all|packages|meta`.
    struct CacheClean
    {
        CacheScope scope;
    };

    /// `cache make`: create repository metadata; with `isForced` (`--force`,
    /// `cache make!`), overwrite it.
    struct CacheMake
    {
        bool isForced = false;
    };

    /// Which repositories `repo list` lists.
    enum class RepositoryFilter
    {
        kAll,
        kEnabled,
        kDisabled,
    };

    /// `repo list [--enabled] [--disabled]`; both, or neither, list all.
    struct RepositoryList
    {
        RepositoryFilter filter = RepositoryFilter::kAll;
    };

    /// `explain <error-code>`.
    struct Explain
    {
        std::string code;
    };

    /// `--help`, `-h`, or no command; `<command> --help` names the command.
    struct Help
    {
        /// The command whose help is asked for; empty for TalaPM's own.
        std::string command;
    };

    /// `--version`.
    struct Version
    {};

    /// `config dump-default`: the default configuration as TOML.
    struct ConfigDumpDefault
    {};

    /// `config validate`: check the configuration.
    struct ConfigValidate
    {};

    /// `config dump`: the configuration in effect as TOML.
    struct ConfigDump
    {};

    /// `config get <setting>`: one setting of the configuration in effect.
    struct ConfigGet
    {
        /// The dotted key, such as `download.parallel`.
        std::string setting;
    };

} // namespace Progmasoft::TalaPM::Commands

namespace Progmasoft::TalaPM
{

    /// One command of TalaPM.
    using Command = std::variant<Commands::Install,
                                 Commands::Remove,
                                 Commands::Update,
                                 Commands::Upgrade,
                                 Commands::AutoRemove,
                                 Commands::Search,
                                 Commands::Info,
                                 Commands::List,
                                 Commands::Policy,
                                 Commands::Provides,
                                 Commands::CacheClean,
                                 Commands::CacheMake,
                                 Commands::RepositoryList,
                                 Commands::Explain,
                                 Commands::Help,
                                 Commands::Version,
                                 Commands::ConfigDumpDefault,
                                 Commands::ConfigValidate,
                                 Commands::ConfigDump,
                                 Commands::ConfigGet>;

    /// `--color`.
    enum class ColorChoice
    {
        kAlways,
        kAuto,
        kNever,
    };

    /// The options every command takes, wherever they stand on the line.
    struct GlobalOptions
    {
        /// `-y`, `--yes`: answer yes to every question.
        bool assumeYes = false;
        /// `--dry-run`: show what would be done and do nothing.
        bool isDryRun = false;
        /// `-q`, `--quiet`.
        bool isQuiet = false;
        /// `-V`, `--verbose`; it wins over the configuration's verbosity.
        bool isVerbose = false;
        /// `--color`; it wins over the configuration's `[color] level`.
        std::optional<ColorChoice> color;
        /// `--root`: the system TalaPM reads and changes, `/` without it.
        std::optional<std::string> root;
        /// `--default-config`: read no configuration file.
        bool useDefaultConfiguration = false;
    };

    /// A command line, read: its command and its `-s` settings.
    struct Invocation
    {
        Command command;
        GlobalOptions options;
        /// The text of each `-s`/`--setting`, in configuration syntax, in the
        /// order given; a later one wins.
        std::vector<std::string> settings;
    };

    /// Reads the arguments after the program name. Options may come anywhere
    /// and combine freely: every option a command has may be given with any
    /// other. A short option takes the text after its letter as its value, so
    /// `-uU` is
    /// `-u` with the value `U`, which `-u` refuses; `-s'...'` is `-s` with its
    /// setting. A word after an option is its value too, so an option that
    /// takes none is followed only by another option, `--` or the end: `install
    /// pkg -U` and `install -U -- pkg`, not `install -U pkg`. `--` ends the
    /// options. `-s` takes its value as the next argument or attached
    /// (`-s'...'`), `--setting` as the next argument or after `=`.
    ///
    /// @param arguments The arguments.
    /// @return The invocation, or kUsage.
    Expected<Invocation>
    ParseCommandLine(const std::vector<std::string_view> &arguments);

    /// Puts the global options over the configuration, last of every source,
    /// so they win over `-s` too: `--quiet` makes the verbosity
    /// "Always-Silent", `--verbose` "Always-Verbose", and `--color` sets the
    /// color level.
    ///
    /// @param options       The global options of the command line.
    /// @param configuration The configuration, every other source applied.
    void
    ApplyGlobalOptions(const GlobalOptions &options,
                       Configuration &configuration);

} // namespace Progmasoft::TalaPM
