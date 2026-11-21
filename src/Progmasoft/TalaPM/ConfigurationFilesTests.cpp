// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MIT

#include <Progmasoft/Catch3/All.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Progmasoft/TalaPM/ConfigurationFiles.hpp"

using Progmasoft::TalaPM::Configuration;
using Progmasoft::TalaPM::ConfigurationSources;
using Progmasoft::TalaPM::ErrorCode;
using Progmasoft::TalaPM::LoadConfiguration;
using Progmasoft::TalaPM::LoadSystemConfiguration;
using Progmasoft::TalaPM::ReadConfigurationSources;

namespace
{

    /// A system root of one test, under the directory Bazel gives tests.
    class Root
    {
    public:
        explicit Root(const std::string &name)
        {
            const char *base = std::getenv("TEST_TMPDIR");
            path_ = std::filesystem::path(base != nullptr ? base : ".") / name;
            std::filesystem::remove_all(path_);
            std::filesystem::create_directories(path_ / "etc/talapm.d");
            std::filesystem::create_directories(path_ / "etc/talapm.conf.d");
        }

        void
        Write(const std::string &relative, const std::string &text) const
        {
            std::filesystem::path file = path_ / relative;
            std::filesystem::create_directories(file.parent_path());
            std::ofstream(file, std::ios::binary) << text;
        }

        const std::filesystem::path &
        Path() const
        {
            return path_;
        }

    private:
        std::filesystem::path path_;
    };

    /// The configuration file this repository installs.
    std::string
    InstalledMain()
    {
        std::ifstream stream("etc/talapm.conf.toml", std::ios::binary);
        REQUIRE(stream.good());
        return std::string((std::istreambuf_iterator<char>(stream)),
                           std::istreambuf_iterator<char>());
    }

} // namespace

TEST_CASE("the installed configuration is the default one")
{
    auto installed = LoadConfiguration(
        ConfigurationSources{ { "base", "" },
                              { "etc/talapm.conf.toml", InstalledMain() },
                              {},
                              {} });
    auto none = LoadConfiguration(ConfigurationSources{});
    REQUIRE(installed.has_value());
    REQUIRE(none.has_value());
    CHECK(installed->download.parallel == none->download.parallel);
    CHECK(installed->download.verbosity == "User-Friendly");
    CHECK(installed->signatureLevels == none->signatureLevels);
    REQUIRE(installed->repositories.size() == 5);
    for (std::size_t index = 0; index < 5; ++index)
    {
        CHECK(installed->repositories[index].name
              == none->repositories[index].name);
        CHECK(installed->repositories[index].isEnabled
              == none->repositories[index].isEnabled);
    }
}

TEST_CASE("a system's files override each other in the order of the notes")
{
    Root root("override");
    root.Write("etc/talapm.conf.d/config.toml", "[download]\nparallel = 1\n");
    root.Write("etc/talapm.conf.toml", InstalledMain());
    root.Write("etc/talapm.d/lower.toml", "[download]\nparallel = 5\n");
    root.Write("etc/talapm.d/Upper.toml", "[download]\nparallel = 6\n");
    root.Write("etc/talapm.d/sub/20-digit.conf.toml",
               "[download]\nparallel = 7\n");
    root.Write("etc/talapm.d/old.toml.conf", "[download]\nparallel = 60\n");
    root.Write("etc/talapm.d/notes.txt", "not toml");

    root.Write("etc/talapm.d/_special.toml", "[download]\nparallel = 8\n");

    // Each step removes the highest source left, and the next one shows.
    auto load = [&](std::vector<std::string> settings) {
        auto configuration = LoadSystemConfiguration(root.Path(), settings);
        REQUIRE(configuration.has_value());
        return configuration->download.parallel;
    };
    // Read as 20-digit, Upper, _special, lower: the last one read wins.
    CHECK(load({ "[download]\nparallel = 9\n" }) == 9);
    CHECK(load({}) == 5);
    std::filesystem::remove(root.Path() / "etc/talapm.d/lower.toml");
    CHECK(load({}) == 8);
    std::filesystem::remove(root.Path() / "etc/talapm.d/_special.toml");
    CHECK(load({}) == 6);
    std::filesystem::remove(root.Path() / "etc/talapm.d/Upper.toml");
    CHECK(load({}) == 7);
    std::filesystem::remove(root.Path()
                            / "etc/talapm.d/sub/20-digit.conf.toml");
    CHECK(load({}) == 4);
    root.Write("etc/talapm.conf.toml", "");
    CHECK(load({}) == 1);
}

TEST_CASE("missing files read as empty")
{
    Root root("missing");
    std::filesystem::remove_all(root.Path() / "etc");
    auto configuration = LoadSystemConfiguration(root.Path(), {});
    REQUIRE(configuration.has_value());
    CHECK(configuration->download.parallel == 4);
    auto sources = ReadConfigurationSources(root.Path(), {});
    REQUIRE(sources.has_value());
    CHECK(sources->dropIns.empty());
}

TEST_CASE("drop-ins are named by their path under the root")
{
    Root root("names");
    root.Write("etc/talapm.d/a/b.toml", "");
    auto sources = ReadConfigurationSources(root.Path(), { "x = 1" });
    REQUIRE(sources.has_value());
    REQUIRE(sources->dropIns.size() == 1);
    CHECK(sources->dropIns[0].path == "etc/talapm.d/a/b.toml");
    CHECK(sources->settings == std::vector<std::string>{ "x = 1" });
}

TEST_CASE("an invalid drop-in is reported with its path")
{
    Root root("invalid");
    root.Write("etc/talapm.d/50-bad.toml", "[download]\nparallel = 0\n");
    auto configuration = LoadSystemConfiguration(root.Path(), {});
    REQUIRE_FALSE(configuration.has_value());
    CHECK(configuration.error().code == ErrorCode::kBadConfiguration);
    CHECK(configuration.error().message.rfind("etc/talapm.d/50-bad.toml: ", 0)
          == 0);
}

TEST_CASE("a file larger than 1 MiB is refused")
{
    Root root("large");
    root.Write("etc/talapm.d/big.toml", std::string(1024 * 1024 + 1, '#'));
    auto sources = ReadConfigurationSources(root.Path(), {});
    REQUIRE_FALSE(sources.has_value());
    CHECK(sources.error().message.find("larger than 1 MiB")
          != std::string::npos);
}

TEST_CASE("a link to a directory is not followed")
{
    Root root("links");
    root.Write("etc/talapm.d/real/a.toml", "");
    std::filesystem::create_directory_symlink(root.Path() / "etc/talapm.d",
                                              root.Path()
                                                  / "etc/talapm.d/real/loop");
    auto sources = ReadConfigurationSources(root.Path(), {});
    REQUIRE(sources.has_value());
    CHECK(sources->dropIns.size() == 1);
}

TEST_CASE("only .toml files of talapm.d are read")
{
    Root root("kinds");
    // Each of these would be refused if it were read: too large and not TOML.
    std::string large(1024 * 1024 + 1, '{');
    for (const char *name : { "etc/talapm.d/a.yaml",
                              "etc/talapm.d/b.yml",
                              "etc/talapm.d/c.json",
                              "etc/talapm.d/d.ini",
                              "etc/talapm.d/e",
                              "etc/talapm.d/f.toml.conf",
                              "etc/talapm.d/g.conf",
                              "etc/talapm.d/h.TOML",
                              "etc/talapm.d/.toml",
                              "etc/talapm.d/i.toml~" })
    {
        root.Write(name, large);
    }
    root.Write("etc/talapm.d/j.conf.toml", "[download]\nparallel = 3\n");
    auto sources = ReadConfigurationSources(root.Path(), {});
    REQUIRE(sources.has_value());
    REQUIRE(sources->dropIns.size() == 1);
    CHECK(sources->dropIns[0].path == "etc/talapm.d/j.conf.toml");
    auto configuration = LoadSystemConfiguration(root.Path(), {});
    REQUIRE(configuration.has_value());
    CHECK(configuration->download.parallel == 3);
}
