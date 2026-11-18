// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MIT

#include <array>
#include <cstdio>
#include <utility>

#include "Progmasoft/TalaPM/Error.hpp"

namespace Progmasoft::TalaPM
{
    namespace
    {

        /// The explanation of each error code.
        constexpr std::array<std::pair<ErrorCode, std::string_view>, 6>
            kExplanations{ {
                { ErrorCode::kUsage,
                  "The command line is not one TalaPM takes: an unknown "
                  "command or "
                  "option, a missing package name, or an option the command "
                  "does not "
                  "have. `talapm --help` lists the commands." },
                { ErrorCode::kBadVersion,
                  "A version is not written as TalaPM reads them: it is empty, "
                  "or it has "
                  "a character other than letters, digits, '.', '+' and '~'." },
                { ErrorCode::kBadManifest,
                  "The .META/MANIFEST.XML of a package is malformed: it is not "
                  "XML, its "
                  "format is not one TalaPM reads, or a field is missing or "
                  "invalid. The "
                  "package is not installed." },
                { ErrorCode::kUnsafePath,
                  "A package names a path that would leave the root: an "
                  "absolute path, a "
                  "'..' component, or an empty one. TalaPM refuses the "
                  "package." },
                { ErrorCode::kBadConfiguration,
                  "A configuration file or a -s setting is invalid: it is not "
                  "TOML, or it "
                  "has a key or a value TalaPM does not take." },
                { ErrorCode::kUnresolvable,
                  "No set of packages meets every relation: a dependency has "
                  "no version "
                  "in the enabled repositories, or the packages asked for "
                  "conflict." },
            } };

    } // namespace

    std::string
    CodeName(ErrorCode code)
    {
        std::array<char, 8> text{};
        std::snprintf(text.data(),
                      text.size(),
                      "TPM%04d",
                      static_cast<int>(code));
        return text.data();
    }

    Expected<std::string_view>
    Explain(std::string_view code)
    {
        for (const auto &[known, explanation] : kExplanations)
        {
            std::string name = CodeName(known);
            if (code == name
                || code == name.substr(name.find_first_not_of("TPM0")))
            {
                return explanation;
            }
        }
        return Fail(ErrorCode::kUsage,
                    "no error code " + std::string(code) + "; codes run from "
                        + CodeName(ErrorCode::kUsage) + " to "
                        + CodeName(ErrorCode::kUnresolvable));
    }

} // namespace Progmasoft::TalaPM
