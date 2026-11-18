// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MIT

#include <Progmasoft/Catch3/All.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "Progmasoft/TalaPM/CommandLine.hpp"

namespace
{

    using Progmasoft::TalaPM::ErrorCode;
    using Progmasoft::TalaPM::Expected;
    using Progmasoft::TalaPM::Invocation;
    using Progmasoft::TalaPM::ParseCommandLine;
    namespace Commands = Progmasoft::TalaPM::Commands;

    Expected<Invocation>
    Parse(std::vector<std::string_view> arguments)
    {
        return ParseCommandLine(arguments);
    }

    template<typename Value>
    Value
    ParseAs(std::vector<std::string_view> arguments)
    {
        Expected<Invocation> invocation = Parse(std::move(arguments));
        REQUIRE(invocation.has_value());
        REQUIRE(std::holds_alternative<Value>(invocation->command));
        return std::get<Value>(invocation->command);
    }

    /// The message of a command line TalaPM refuses.
    std::string
    Refusal(std::vector<std::string_view> arguments)
    {
        Expected<Invocation> invocation = Parse(std::move(arguments));
        REQUIRE_FALSE(invocation.has_value());
        CHECK(invocation.error().code == ErrorCode::kUsage);
        return invocation.error().message;
    }

} // namespace

TEST_CASE("install takes packages and combines its flags")
{
    Commands::Install install
        = ParseAs<Commands::Install>({ "install",
                                       "tala-utils",
                                       "opencrt",
                                       "-u",
                                       "--upgrade",
                                       "--needs",
                                       "--with-recommendeds" });
    CHECK(install.packages
          == std::vector<std::string>{ "tala-utils", "opencrt" });
    CHECK(install.shouldUpdate);
    CHECK(install.shouldUpgrade);
    CHECK(install.isNeedsOnly);
    CHECK(install.withRecommended);

    Commands::Install plain = ParseAs<Commands::Install>({ "install", "x" });
    CHECK_FALSE(plain.shouldUpdate);
    CHECK_FALSE(plain.shouldUpgrade);
    CHECK(Refusal({ "install" }).find("needs a package") != std::string::npos);
    CHECK(Refusal({ "install", "x", "--check" }).find("--check")
          != std::string::npos);
}

TEST_CASE("a word after an option that takes no value is refused")
{
    CHECK(Refusal({ "install", "-U", "pkg" })
          == "-U takes no value, but was given 'pkg'; put the operands before "
             "it, "
             "or after --");
    Refusal({ "install", "--needs", "pkg" });
    Refusal({ "-u", "install", "pkg" });
    CHECK(ParseAs<Commands::Install>({ "install", "pkg", "-U" }).shouldUpgrade);
    Commands::Install separated
        = ParseAs<Commands::Install>({ "install", "-U", "--", "pkg" });
    CHECK(separated.shouldUpgrade);
    CHECK(separated.packages == std::vector<std::string>{ "pkg" });
    Commands::Install bare
        = ParseAs<Commands::Install>({ "install", "--", "pkg" });
    CHECK(bare.packages == std::vector<std::string>{ "pkg" });
    CHECK_FALSE(bare.shouldUpgrade);
}

TEST_CASE("a short flag takes what follows its letter as its value")
{
    CHECK(Refusal({ "install", "x", "-uU" })
          == "-u takes no value, but was given 'U'");
    Commands::Install both
        = ParseAs<Commands::Install>({ "install", "x", "-u", "-U" });
    CHECK((both.shouldUpdate && both.shouldUpgrade));
}

TEST_CASE("remove and purge")
{
    Commands::Remove remove = ParseAs<Commands::Remove>({ "remove", "a", "b" });
    CHECK_FALSE(remove.isPurge);
    CHECK(remove.packages.size() == 2);
    CHECK(ParseAs<Commands::Remove>({ "purge", "a" }).isPurge);
    Refusal({ "purge" });
}

TEST_CASE("update, upgrade and full-upgrade, with their flags together")
{
    CHECK(ParseAs<Commands::Update>({ "update", "-U" }).shouldUpgrade);
    CHECK(ParseAs<Commands::Update>({ "update", "--upgrade" }).shouldUpgrade);
    CHECK_FALSE(ParseAs<Commands::Update>({ "update" }).shouldUpgrade);

    Commands::Upgrade check
        = ParseAs<Commands::Upgrade>({ "upgrade", "--check", "-u" });
    CHECK((check.isCheckOnly && check.shouldUpdate && !check.isFull));
    CHECK(ParseAs<Commands::Upgrade>({ "upgrade", "opencrt" }).packages
          == std::vector<std::string>{ "opencrt" });

    Commands::Upgrade full
        = ParseAs<Commands::Upgrade>({ "full-upgrade", "--update", "--check" });
    CHECK((full.isFull && full.shouldUpdate && full.isCheckOnly));
    Refusal({ "full-upgrade", "opencrt" });
    Refusal({ "update", "x" });
}

TEST_CASE("list takes its kinds together")
{
    using Progmasoft::TalaPM::Commands::ListKind;
    Commands::List system = ParseAs<Commands::List>(
        { "list", "--installed", "--upgradable", "--installed" });
    CHECK(system.kinds
          == std::vector<ListKind>{ ListKind::kInstalled,
                                    ListKind::kUpgradable });
    CHECK(system.package.empty());

    Commands::List package = ParseAs<Commands::List>(
        { "list", "opencrt", "--versions", "--depends", "--rdepends" });
    CHECK(package.kinds.size() == 3);
    CHECK(package.package == "opencrt");

    Commands::List narrowed
        = ParseAs<Commands::List>({ "list", "opencrt", "--installed" });
    CHECK(narrowed.package == "opencrt");

    Refusal({ "list" });
    Refusal({ "list", "--versions" });
    Refusal({ "list", "a", "b", "--installed" });
}

TEST_CASE("commands of one operand")
{
    CHECK(ParseAs<Commands::Search>({ "search", "sudo" }).text == "sudo");
    CHECK(ParseAs<Commands::Info>({ "info", "opencrt" }).package == "opencrt");
    CHECK(ParseAs<Commands::Policy>({ "policy", "x" }).package == "x");
    CHECK(ParseAs<Commands::Provides>({ "provides", "usr/bin/ls" }).path
          == "usr/bin/ls");
    CHECK(ParseAs<Commands::Explain>({ "explain", "TPM0001" }).code
          == "TPM0001");
    Refusal({ "search" });
    Refusal({ "info", "a", "b" });
    ParseAs<Commands::AutoRemove>({ "auto-remove" });
    Refusal({ "auto-remove", "x" });
}

TEST_CASE("cache clean and cache make")
{
    using Progmasoft::TalaPM::Commands::CacheScope;
    CHECK(ParseAs<Commands::CacheClean>({ "cache", "clean", "all" }).scope
          == CacheScope::kAll);
    CHECK(ParseAs<Commands::CacheClean>({ "cache", "clean", "packages" }).scope
          == CacheScope::kPackages);
    CHECK(ParseAs<Commands::CacheClean>({ "cache", "clean", "meta" }).scope
          == CacheScope::kMeta);
    CHECK_FALSE(ParseAs<Commands::CacheMake>({ "cache", "make" }).isForced);
    CHECK(
        ParseAs<Commands::CacheMake>({ "cache", "make", "--force" }).isForced);
    CHECK(ParseAs<Commands::CacheMake>({ "cache", "make!" }).isForced);
    Refusal({ "cache" });
    Refusal({ "cache", "clean" });
    Refusal({ "cache", "clean", "everything" });
    Refusal({ "cache", "make", "x" });
}

TEST_CASE("repo list filters, and both filters list all")
{
    using Progmasoft::TalaPM::Commands::RepositoryFilter;
    CHECK(ParseAs<Commands::RepositoryList>({ "repo", "list" }).filter
          == RepositoryFilter::kAll);
    CHECK(ParseAs<Commands::RepositoryList>({ "repo", "list", "--enabled" })
              .filter
          == RepositoryFilter::kEnabled);
    CHECK(ParseAs<Commands::RepositoryList>({ "repo", "list", "--disabled" })
              .filter
          == RepositoryFilter::kDisabled);
    CHECK(ParseAs<Commands::RepositoryList>(
              { "repo", "list", "--enabled", "--disabled" })
              .filter
          == RepositoryFilter::kAll);
    Refusal({ "repo" });
    Refusal({ "repo", "list", "core" });
}

TEST_CASE("-s and --setting in all four forms, anywhere")
{
    const char *kSetting = "\n[repo.built-in.nofree]\nenabled = true\n";
    std::string attached = std::string("-s") + kSetting;
    std::string equals = std::string("--setting=") + kSetting;
    for (std::vector<std::string_view> arguments :
         { std::vector<std::string_view>{ "-s", kSetting, "update" },
           std::vector<std::string_view>{ "update", attached },
           std::vector<std::string_view>{ "update", "--setting", kSetting },
           std::vector<std::string_view>{ equals, "update" } })
    {
        Expected<Invocation> invocation = Parse(arguments);
        REQUIRE(invocation.has_value());
        CHECK(std::holds_alternative<Commands::Update>(invocation->command));
        CHECK(invocation->settings == std::vector<std::string>{ kSetting });
    }
    Expected<Invocation> two
        = Parse({ "-s", "a = 1", "update", "--setting=b = 2" });
    REQUIRE(two.has_value());
    CHECK(two->settings == std::vector<std::string>{ "a = 1", "b = 2" });
    CHECK(Refusal({ "update", "-s" }) == "-s needs a setting");
    Refusal({ "update", "--setting" });
}

TEST_CASE("help, unknown commands and the end of options")
{
    CHECK(std::holds_alternative<Commands::Help>(Parse({})->command));
    CHECK(std::holds_alternative<Commands::Help>(Parse({ "--help" })->command));
    CHECK(std::holds_alternative<Commands::Help>(
        Parse({ "install", "x", "-h" })->command));
    CHECK(Refusal({ "frobnicate" }) == "no command 'frobnicate'");
    CHECK(Refusal({ "--update" }) == "no command before --update");

    Commands::Install dashed
        = ParseAs<Commands::Install>({ "install", "--", "-weird-name" });
    CHECK(dashed.packages == std::vector<std::string>{ "-weird-name" });
}
