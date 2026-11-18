// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <system_error>

#include "Progmasoft/TalaPM/Version.hpp"

namespace Progmasoft::TalaPM
{
    namespace
    {

        bool
        IsDigit(char character)
        {
            return character >= '0' && character <= '9';
        }

        bool
        IsLetter(char character)
        {
            return (character >= 'a' && character <= 'z')
                   || (character >= 'A' && character <= 'Z');
        }

        bool
        IsVersionCharacter(char character)
        {
            return IsDigit(character) || IsLetter(character) || character == '.'
                   || character == '+' || character == '~';
        }

        /// Skips the separators that only end a run: `.` and `+`.
        void
        SkipSeparators(std::string_view text, std::size_t &index)
        {
            while (index < text.size()
                   && (text[index] == '.' || text[index] == '+'))
            {
                ++index;
            }
        }

        /// Takes a run of characters of one kind.
        std::string_view
        TakeRun(std::string_view text,
                std::size_t &index,
                bool (*isOfKind)(char))
        {
            std::size_t start = index;
            while (index < text.size() && isOfKind(text[index]))
            {
                ++index;
            }
            return text.substr(start, index - start);
        }

        /// Compares two runs of digits as numbers of any length.
        int
        CompareNumbers(std::string_view left, std::string_view right)
        {
            left.remove_prefix(
                std::min(left.find_first_not_of('0'), left.size()));
            right.remove_prefix(
                std::min(right.find_first_not_of('0'), right.size()));
            if (left.size() != right.size())
            {
                return left.size() < right.size() ? -1 : 1;
            }
            return left.compare(right);
        }

        /// The rank of what comes next: `~` below the end, the end below
        /// letters, letters below digits.
        int
        RankAt(std::string_view text, std::size_t index)
        {
            if (index >= text.size())
            {
                return 1;
            }
            if (text[index] == '~')
            {
                return 0;
            }
            return IsDigit(text[index]) ? 3 : 2;
        }

        int
        Sign(int value)
        {
            return (value > 0) - (value < 0);
        }

    } // namespace

    Expected<Version>
    Version::Parse(std::string_view text)
    {
        if (text.empty() || !(IsDigit(text[0]) || IsLetter(text[0])))
        {
            return Fail(ErrorCode::kBadVersion,
                        "version '" + std::string(text)
                            + "' must start with a letter or a digit");
        }
        for (char character : text)
        {
            if (!IsVersionCharacter(character))
            {
                return Fail(
                    ErrorCode::kBadVersion,
                    "version '" + std::string(text)
                        + "' has a character other than letters, digits, "
                          "'.', '+' and '~'");
            }
        }
        return Version(std::string(text));
    }

    int
    Version::Compare(const Version &other) const
    {
        std::string_view left = text_;
        std::string_view right = other.text_;
        std::size_t leftIndex = 0;
        std::size_t rightIndex = 0;
        while (true)
        {
            SkipSeparators(left, leftIndex);
            SkipSeparators(right, rightIndex);
            int leftRank = RankAt(left, leftIndex);
            int rightRank = RankAt(right, rightIndex);
            if (leftRank != rightRank)
            {
                return leftRank < rightRank ? -1 : 1;
            }
            if (leftRank == 1)
            {
                return 0;
            }
            if (leftRank == 0)
            {
                ++leftIndex;
                ++rightIndex;
                continue;
            }
            bool isNumber = leftRank == 3;
            std::string_view leftRun
                = TakeRun(left, leftIndex, isNumber ? IsDigit : IsLetter);
            std::string_view rightRun
                = TakeRun(right, rightIndex, isNumber ? IsDigit : IsLetter);
            int order = isNumber ? CompareNumbers(leftRun, rightRun)
                                 : Sign(leftRun.compare(rightRun));
            if (order != 0)
            {
                return order;
            }
        }
    }

    Expected<PackageVersion>
    PackageVersion::Parse(std::string_view text)
    {
        std::size_t dash = text.rfind('-');
        if (dash == std::string_view::npos)
        {
            return Fail(
                ErrorCode::kBadVersion,
                "package version '" + std::string(text)
                    + "' has no release; it is written <version>-<release>");
        }
        std::string_view releaseText = text.substr(dash + 1);
        std::uint32_t release = 0;
        auto [end, error]
            = std::from_chars(releaseText.data(),
                              releaseText.data() + releaseText.size(),
                              release);
        if (releaseText.empty() || error != std::errc()
            || end != releaseText.data() + releaseText.size() || release == 0)
        {
            return Fail(ErrorCode::kBadVersion,
                        "release '" + std::string(releaseText)
                            + "' is not a whole number from 1");
        }
        Expected<Version> version = Version::Parse(text.substr(0, dash));
        if (!version)
        {
            return tl::unexpected(version.error());
        }
        return PackageVersion{ std::move(*version), release };
    }

    std::string
    PackageVersion::Text() const
    {
        return version.Text() + "-" + std::to_string(release);
    }

    int
    PackageVersion::Compare(const PackageVersion &other) const
    {
        int order = version.Compare(other.version);
        if (order != 0)
        {
            return order;
        }
        return release == other.release ? 0
                                        : (release < other.release ? -1 : 1);
    }

} // namespace Progmasoft::TalaPM
