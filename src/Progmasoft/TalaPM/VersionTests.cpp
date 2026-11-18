// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MIT

#include <Progmasoft/Catch3/All.hpp>

#include "Progmasoft/TalaPM/Version.hpp"

using Progmasoft::TalaPM::ErrorCode;
using Progmasoft::TalaPM::PackageVersion;
using Progmasoft::TalaPM::Version;

namespace
{

    int
    Compare(const char *left, const char *right)
    {
        return Version::Parse(left)->Compare(*Version::Parse(right));
    }

} // namespace

TEST_CASE("versions compare by runs of digits and letters")
{
    CHECK(Compare("1.10", "1.9") > 0);
    CHECK(Compare("1.0.1", "1.0") > 0);
    CHECK(Compare("1.0", "1.0") == 0);
    CHECK(Compare("1.00", "1.0") == 0);
    CHECK(Compare("1.0a", "1.0") > 0);
    CHECK(Compare("1.0a", "1.0b") < 0);
    CHECK(Compare("1.0.1", "1.0a") > 0);
    CHECK(Compare("1.0~rc1", "1.0") < 0);
    CHECK(Compare("1.0~rc1", "1.0~rc2") < 0);
    CHECK(Compare("1.0~~", "1.0~") < 0);
    CHECK(Compare("2.3.1+git5", "2.3.1") > 0);
    CHECK(Compare("123456789012345678901234567890", "99") > 0);
}

TEST_CASE("only letters, digits, '.', '+' and '~' make a version")
{
    for (const char *bad : { "", ".1", "~1", "1 0", "1/0", "1-0", "1:0" })
    {
        auto version = Version::Parse(bad);
        REQUIRE_FALSE(version.has_value());
        CHECK(version.error().code == ErrorCode::kBadVersion);
    }
}

TEST_CASE("package versions add a release")
{
    auto version = PackageVersion::Parse("1.0.0-2");
    REQUIRE(version.has_value());
    CHECK(version->version.Text() == "1.0.0");
    CHECK(version->release == 2);
    CHECK(version->Text() == "1.0.0-2");
    CHECK(version->Compare(*PackageVersion::Parse("1.0.0-10")) < 0);
    CHECK(version->Compare(*PackageVersion::Parse("1.0.1-1")) < 0);
    for (const char *bad :
         { "1.0.0", "1.0.0-", "1.0.0-0", "1.0.0-x", "-1", "1.0.0-99999999999" })
    {
        CHECK_FALSE(PackageVersion::Parse(bad).has_value());
    }
}
