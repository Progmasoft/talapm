// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MIT

#include <Progmasoft/Catch3/All.hpp>
#include <string>
#include <vector>

#include "Progmasoft/TalaPM/Configuration.hpp"

using Progmasoft::TalaPM::ConfigurationFile;
using Progmasoft::TalaPM::ConfigurationSources;
using Progmasoft::TalaPM::ErrorCode;
using Progmasoft::TalaPM::LoadConfiguration;
using Progmasoft::TalaPM::OrderDropIns;

namespace
{

    /// The default etc/talapm.conf.toml of the TalaPM notes.
    const char *kDefault = R"([download]
parallel = 4
weak-deps = false
keep-cache = true
check-space = true
verbosity = "User-Friendly"

[signature]
level = ["Recommend", "Database"]

[repo.built-in.core]
enabled = true

[repo.built-in.extra]
enabled = true

[repo.built-in.lib32]
enabled = false

[repo.built-in.nofree]
enabled = false

[repo.built-in.lib32nofree]
enabled = false
)";

    ConfigurationSources
    Sources()
    {
        return ConfigurationSources{ { "talapm.conf.d/config.toml", "" },
                                     { "talapm.conf.toml", kDefault },
                                     {},
                                     {} };
    }

    std::vector<std::string>
    Names(const std::vector<ConfigurationFile> &files)
    {
        std::vector<std::string> names;
        for (const ConfigurationFile &file : files)
        {
            names.push_back(file.path);
        }
        return names;
    }

} // namespace

TEST_CASE("the default configuration reads")
{
    auto configuration = LoadConfiguration(Sources());
    REQUIRE(configuration.has_value());
    CHECK(configuration->download.parallel == 4);
    CHECK(configuration->download.keepCache);
    CHECK(configuration->signatureLevels
          == std::vector<std::string>{ "Recommend", "Database" });
    REQUIRE(configuration->repositories.size() == 5);
    CHECK(configuration->repositories[0].name == "core");
    CHECK(configuration->repositories[0].isEnabled);
    CHECK_FALSE(configuration->repositories[3].isEnabled);
}

TEST_CASE("drop-ins count by their names, highest first")
{
    std::vector<ConfigurationFile> files{
        { "talapm.d/b.toml", "" },        { "talapm.d/A.toml", "" },
        { "talapm.d/10-x.toml", "" },     { "talapm.d/00-y.conf.toml", "" },
        { "talapm.d/_z.toml", "" },       { "talapm.d/sub/a.toml", "" },
        { "talapm.d/old.toml.conf", "" }, { "talapm.d/notes.txt", "" },
    };
    CHECK(Names(OrderDropIns(files))
          == std::vector<std::string>{ "talapm.d/_z.toml",
                                       "talapm.d/00-y.conf.toml",
                                       "talapm.d/10-x.toml",
                                       "talapm.d/A.toml",
                                       "talapm.d/sub/a.toml",
                                       "talapm.d/b.toml" });
}

TEST_CASE("higher sources win, and -s is the highest")
{
    ConfigurationSources sources = Sources();
    sources.dropIns
        = { { "talapm.d/50-low.toml", "[download]\nparallel = 2\n" },
            { "talapm.d/10-high.toml", "[download]\nparallel = 8\n" } };
    sources.settings = { "[repo.built-in.nofree]\nenabled = true\n",
                         "[download]\nparallel = 16\n" };
    auto configuration = LoadConfiguration(sources);
    REQUIRE(configuration.has_value());
    CHECK(configuration->download.parallel == 16);
    CHECK(configuration->repositories[3].isEnabled);

    sources.settings.clear();
    CHECK(LoadConfiguration(sources)->download.parallel == 8);
}

TEST_CASE("new repositories keep the order they are written in")
{
    ConfigurationSources sources = Sources();
    sources.dropIns
        = { { "talapm.d/mirrors.toml",
              "[repo.zeta]\nenabled = true\nmirror-uris = "
              "[\"https://b.example\", \"https://a.example\"]\n"
              "[repo.alpha]\nmirror-uris = [\"https://a.example\"]\n" } };
    auto configuration = LoadConfiguration(sources);
    REQUIRE(configuration.has_value());
    REQUIRE(configuration->repositories.size() == 7);
    CHECK(configuration->repositories[5].name == "zeta");
    CHECK(configuration->repositories[6].name == "alpha");
    CHECK(configuration->repositories[5].mirrorUris
          == std::vector<std::string>{ "https://b.example",
                                       "https://a.example" });
}

TEST_CASE("invalid configuration is refused, naming its source")
{
    for (const char *text :
         { "[download]\nparallel = 0\n",
           "[download]\nparallel = \"4\"\n",
           "[download]\nspeed = 1\n",
           "[colors]\n",
           "[repo.built-in.core]\nenabled = 1\n",
           "[repo.built-in.x]\nenabled = true\n",
           "[repo.built-in.core]\nmirror-uris = "
           "[\"https://elsewhere.example\"]\n",
           "[repo.x]\nmirror-uris = [\"http://plain.example\"]\n",
           "[repo.x]\nmirror-uris = []\n",
           "[repo.x]\nenabled = true\n",
           "[repo.core]\nmirror-uris = [\"https://a.example\"]\n",
           "[repo.other.x]\nenabled = true\n",
           "[download]\nverbosity = \"Loud\"\n",
           "[color]\nlevel = \"Sometimes\"\n",
           "[color]\nlevel = 1\n",
           "[color]\nshade = \"Auto\"\n",
           "[signature]\nlevel = [1]\n",
           "not toml [" })
    {
        ConfigurationSources sources = Sources();
        sources.settings = { text };
        auto configuration = LoadConfiguration(sources);
        REQUIRE_FALSE(configuration.has_value());
        CHECK(configuration.error().code == ErrorCode::kBadConfiguration);
        CHECK(configuration.error().message.rfind("-s #1: ", 0) == 0);
    }
}

TEST_CASE("verbosity takes its five values, and color its three")
{
    for (std::string_view verbosity : Progmasoft::TalaPM::kVerbosities)
    {
        ConfigurationSources sources = Sources();
        sources.settings = { "[download]\nverbosity = \""
                             + std::string(verbosity) + "\"\n" };
        auto configuration = LoadConfiguration(sources);
        REQUIRE(configuration.has_value());
        CHECK(configuration->download.verbosity == verbosity);
    }
    for (const auto &[text, level] :
         { std::pair{ "Always", Progmasoft::TalaPM::ColorLevel::kAlways },
           std::pair{ "Never", Progmasoft::TalaPM::ColorLevel::kNever },
           std::pair{ "Auto", Progmasoft::TalaPM::ColorLevel::kAuto } })
    {
        ConfigurationSources sources = Sources();
        sources.settings
            = { std::string("[color]\nlevel = \"") + text + "\"\n" };
        auto configuration = LoadConfiguration(sources);
        REQUIRE(configuration.has_value());
        CHECK(configuration->color == level);
    }
}

TEST_CASE("a dump reads back as the same configuration, and get takes its keys")
{
    ConfigurationSources sources = Sources();
    sources.settings
        = { "[color]\nlevel = \"Never\"\n[repo.example]\nenabled = "
            "true\nmirror-uris = [\"https://repo.example.org/\"]\n" };
    auto configuration = LoadConfiguration(sources);
    REQUIRE(configuration.has_value());
    std::string dump = Progmasoft::TalaPM::DumpConfiguration(*configuration);
    CHECK(dump.find("[repo.example]\nenabled = true\nmirror-uris = "
                    "[\"https://repo.example.org/\"]\n")
          != std::string::npos);
    CHECK(dump.find("package.tala.run") == std::string::npos);

    ConfigurationSources again;
    again.settings = { dump };
    auto reread = LoadConfiguration(again);
    REQUIRE(reread.has_value());
    CHECK(Progmasoft::TalaPM::DumpConfiguration(*reread) == dump);

    using Progmasoft::TalaPM::GetSetting;
    CHECK(GetSetting(*configuration, "download.parallel").value() == "4");
    CHECK(GetSetting(*configuration, "color.level").value() == "\"Never\"");
    CHECK(GetSetting(*configuration, "signature.level").value()
          == "[\"Recommend\", \"Database\"]");
    CHECK(GetSetting(*configuration, "repo.built-in.core.enabled").value()
          == "true");
    CHECK(GetSetting(*configuration, "repo.example.mirror-uris").value()
          == "[\"https://repo.example.org/\"]");
    CHECK(GetSetting(*configuration, "repo.built-in.core.mirror-uris")
              .error()
              .code
          == ErrorCode::kUsage);
    CHECK(GetSetting(*configuration, "download").error().code
          == ErrorCode::kUsage);
}

TEST_CASE("the defaults are the installed file, and built-in mirrors are "
          "TalaPM's own")
{
    auto defaults = Progmasoft::TalaPM::DefaultConfiguration();
    REQUIRE(defaults.repositories.size() == 5);
    for (const auto &repository : defaults.repositories)
    {
        CHECK(repository.isBuiltIn);
        CHECK(repository.mirrorUris
              == std::vector<std::string>{
                  std::string(Progmasoft::TalaPM::kBuiltInMirror) });
    }
    ConfigurationSources dumped;
    dumped.settings = { Progmasoft::TalaPM::DumpConfiguration(defaults) };
    REQUIRE(LoadConfiguration(dumped).has_value());
}
