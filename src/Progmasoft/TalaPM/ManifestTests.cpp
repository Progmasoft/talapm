// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MIT

#include <Progmasoft/Catch3/All.hpp>
#include <string>

#include "Progmasoft/TalaPM/Manifest.hpp"

using Progmasoft::TalaPM::Architecture;
using Progmasoft::TalaPM::EntryKind;
using Progmasoft::TalaPM::ErrorCode;
using Progmasoft::TalaPM::IsPackageName;
using Progmasoft::TalaPM::ParseManifest;
using Progmasoft::TalaPM::RelationKind;
using Progmasoft::TalaPM::Version;

namespace
{

    const std::string kPackage = R"(<package>
    <name>tala-utils</name>
    <version>1.0.0</version>
    <release>1</release>
    <architecture>amd64</architecture>
    <summary>BSD utilities and tools such as sudo</summary>
    <license>MIT</license>
    <packager>Progmasoft &lt;support@progmasoft.com&gt;</packager>
    <built>2027-05-01T12:00:00Z</built>
    <installed-size>1843200</installed-size>
  </package>)";

    std::string
    Wrap(const std::string &body)
    {
        return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<manifest "
               "format=\"1\">"
               + body + "</manifest>";
    }

    ErrorCode
    RefusalOf(const std::string &xml)
    {
        auto manifest = ParseManifest(xml);
        REQUIRE_FALSE(manifest.has_value());
        return manifest.error().code;
    }

} // namespace

TEST_CASE("the manifest of the README reads")
{
    std::string xml = Wrap(kPackage + R"(
  <relations>
    <depends name="opencrt" at-least="1.0.0" below="2.0.0"/>
  </relations>
  <files>
    <directory path="usr/share/tala-utils" mode="0755" owner="root" group="wheel"/>
    <file path="usr/bin/sudo" mode="4755" owner="root" group="wheel" size="163840"
          sha256="60303ae22b998861bce3b28f33eec1be758a213c86c93c076dbe9f558c11c752"/>
    <file path="etc/sudoers" mode="0440" owner="root" group="wheel" size="1024"
          sha256="fd61a03af4f77d870fc21e05e7e80678095c92d808cfb3b5c279ee04c74aca13" config="true"/>
    <symlink path="usr/bin/[" target="test"/>
  </files>
  <scripts>
    <script phase="post-install" path=".META/SCRIPTS/post-install"/>
  </scripts>)");
    auto manifest = ParseManifest(xml);
    REQUIRE(manifest.has_value());
    CHECK(manifest->name == "tala-utils");
    CHECK(manifest->version.Text() == "1.0.0-1");
    CHECK(manifest->architecture == Architecture::kAmd64);
    CHECK(manifest->packager == "Progmasoft <support@progmasoft.com>");
    CHECK(manifest->installedSizeBytes == 1843200);
    REQUIRE(manifest->relations.size() == 1);
    const auto &depends = manifest->relations[0];
    CHECK(depends.kind == RelationKind::kDepends);
    CHECK(depends.Admits(*Version::Parse("1.4")));
    CHECK_FALSE(depends.Admits(*Version::Parse("2.0.0")));
    CHECK_FALSE(depends.Admits(*Version::Parse("0.9")));
    REQUIRE(manifest->entries.size() == 4);
    CHECK(manifest->entries[1].mode == 04755);
    CHECK(manifest->entries[2].isConfiguration);
    CHECK(manifest->entries[3].kind == EntryKind::kSymlink);
    CHECK(manifest->entries[3].target == "test");
    REQUIRE(manifest->scripts.size() == 1);
}

TEST_CASE("paths that leave the root are refused")
{
    for (const char *path : { "/etc/passwd",
                              "../etc",
                              "usr/../../etc",
                              "usr//bin",
                              "usr/./bin",
                              "" })
    {
        std::string xml = Wrap(kPackage + "<files><directory path=\"" + path
                               + "\" mode=\"0755\"/></files>");
        CHECK(RefusalOf(xml) == ErrorCode::kUnsafePath);
    }
    std::string script = Wrap(kPackage
                              + "<scripts><script phase=\"post-install\" "
                                "path=\"usr/bin/evil\"/></scripts>");
    CHECK(RefusalOf(script) == ErrorCode::kUnsafePath);
}

TEST_CASE("a manifest is data: no DTD, no entities, no network")
{
    std::string entity = "<?xml version=\"1.0\"?>\n"
                         "<!DOCTYPE manifest [<!ENTITY x SYSTEM "
                         "\"file:///etc/passwd\">]>\n<manifest format=\"1\">"
                         + kPackage + "</manifest>";
    CHECK(RefusalOf(entity) == ErrorCode::kBadManifest);
}

TEST_CASE("malformed manifests are refused")
{
    CHECK(RefusalOf("not xml") == ErrorCode::kBadManifest);
    CHECK(RefusalOf("<manifest format=\"2\">" + kPackage + "</manifest>")
          == ErrorCode::kBadManifest);
    CHECK(RefusalOf(Wrap("")) == ErrorCode::kBadManifest);
    CHECK(RefusalOf(Wrap(kPackage + kPackage)) == ErrorCode::kBadManifest);
    std::string noArchitecture = kPackage;
    noArchitecture.replace(noArchitecture.find("amd64"), 5, "sparc");
    CHECK(RefusalOf(Wrap(noArchitecture)) == ErrorCode::kBadManifest);
    CHECK(RefusalOf(Wrap(kPackage
                         + "<files><file path=\"a\" mode=\"0644\" "
                           "size=\"1\" sha256=\"00\"/></files>"))
          == ErrorCode::kBadManifest);
    CHECK(RefusalOf(Wrap(kPackage
                         + "<files><file path=\"a\" mode=\"9\"/>"
                           "</files>"))
          == ErrorCode::kBadManifest);
    CHECK(RefusalOf(Wrap(kPackage
                         + "<relations><depends name=\"Bad\"/>"
                           "</relations>"))
          == ErrorCode::kBadManifest);
}

TEST_CASE("package names")
{
    CHECK(IsPackageName("tala-utils"));
    CHECK(IsPackageName("libstdc++"));
    CHECK(IsPackageName("0ad"));
    CHECK_FALSE(IsPackageName(""));
    CHECK_FALSE(IsPackageName("-x"));
    CHECK_FALSE(IsPackageName("Upper"));
    CHECK_FALSE(IsPackageName("a b"));
    CHECK_FALSE(IsPackageName("a/b"));
}
