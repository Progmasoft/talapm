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

TEST_CASE("drop-ins are read by the bytes of their names, the lowest first")
{
    std::vector<ConfigurationFile> files{
        { "talapm.d/b.toml", "" },        { "talapm.d/A.toml", "" },
        { "talapm.d/10-x.toml", "" },     { "talapm.d/00-y.conf.toml", "" },
        { "talapm.d/_z.toml", "" },       { "talapm.d/sub/a.toml", "" },
        { "talapm.d/old.toml.conf", "" }, { "talapm.d/notes.txt", "" },
    };
    CHECK(Names(OrderDropIns(files))
          == std::vector<std::string>{ "talapm.d/00-y.conf.toml",
                                       "talapm.d/10-x.toml",
                                       "talapm.d/A.toml",
                                       "talapm.d/_z.toml",
                                       "talapm.d/sub/a.toml",
                                       "talapm.d/b.toml" });
}

TEST_CASE("higher sources win, and -s is the highest")
{
    ConfigurationSources sources = Sources();
    sources.dropIns
        = { { "talapm.d/90-high.toml", "[download]\nparallel = 8\n" },
            { "talapm.d/10-low.toml", "[download]\nparallel = 2\n" } };
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
    sources.dropIns = { { "talapm.d/mirrors.toml",
                          "[repo.built-in.zeta]\nenabled = true\nmirror-uris = "
                          "[\"https://b.example\", \"https://a.example\"]\n"
                          "[repo.built-in.alpha]\nenabled = true\n" } };
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
           "[repo.built-in.x]\nenabled = 1\n",
           "[repo.built-in.x]\nmirror-uris = [\"http://plain.example\"]\n",
           "[repo.other.x]\nenabled = true\n",
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
