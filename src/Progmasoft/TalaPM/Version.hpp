// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "Progmasoft/TalaPM/Error.hpp"

namespace Progmasoft::TalaPM
{

    /// The version of a piece of software, as its authors write it: `1.0.0`,
    /// `2.3.1+git5`, `1.0~rc1`.
    ///
    /// Versions compare by runs of digits and runs of letters, left to right.
    /// Two runs of digits compare as numbers, so `1.10` is above `1.9`; two
    /// runs of letters compare by their bytes. A run of digits is above a run
    /// of letters, and either is above nothing, so `1.0.1` is above `1.0`. A
    /// `~` sorts below everything, even the end of the text: `1.0~rc1` is below
    /// `1.0`. Other separators, `.` and `+`, only end a run.
    class Version
    {
    public:
        /// The version 0.
        Version()
            : text_("0")
        {}

        /// Reads a version.
        ///
        /// @param text The version: letters, digits, `.`, `+` and `~`, starting
        ///             with a letter or a digit.
        /// @return The version, or kBadVersion.
        static Expected<Version>
        Parse(std::string_view text);

        /// The version as it was written.
        const std::string &
        Text() const
        {
            return text_;
        }

        /// Compares two versions.
        ///
        /// @param other The other version.
        /// @return Below 0 when this one is lower, 0 when they are equal, above
        ///         0 when this one is higher.
        int
        Compare(const Version &other) const;

        friend bool
        operator==(const Version &left, const Version &right)
        {
            return left.Compare(right) == 0;
        }

        friend bool
        operator!=(const Version &left, const Version &right)
        {
            return left.Compare(right) != 0;
        }

        friend bool
        operator<(const Version &left, const Version &right)
        {
            return left.Compare(right) < 0;
        }

        friend bool
        operator<=(const Version &left, const Version &right)
        {
            return left.Compare(right) <= 0;
        }

        friend bool
        operator>(const Version &left, const Version &right)
        {
            return left.Compare(right) > 0;
        }

        friend bool
        operator>=(const Version &left, const Version &right)
        {
            return left.Compare(right) >= 0;
        }

    private:
        explicit Version(std::string text)
            : text_(std::move(text))
        {}

        std::string text_;
    };

    /// The version of a package: the software's version and the release, which
    /// counts the packagings of that version. It is written `1.0.0-1`.
    struct PackageVersion
    {
        Version version;
        std::uint32_t release;

        /// Reads a package version written `<version>-<release>`.
        ///
        /// @param text The text.
        /// @return The package version, or kBadVersion.
        static Expected<PackageVersion>
        Parse(std::string_view text);

        /// The text, `<version>-<release>`.
        std::string
        Text() const;

        /// Compares by version, then by release.
        ///
        /// @param other The other package version.
        /// @return Below 0, 0 or above 0, as Version::Compare.
        int
        Compare(const PackageVersion &other) const;
    };

} // namespace Progmasoft::TalaPM
