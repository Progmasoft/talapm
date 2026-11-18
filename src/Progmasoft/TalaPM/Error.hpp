// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <string_view>
#include <tl/expected.hpp>
#include <utility>

namespace Progmasoft::TalaPM
{

    /// The error codes of TalaPM; `talapm explain <code>` explains each. A code
    /// keeps its number once released.
    enum class ErrorCode
    {
        kUsage = 1,       ///< The command line is not one TalaPM takes.
        kBadVersion = 2,  ///< A version is not written as TalaPM reads them.
        kBadManifest = 3, ///< A package manifest is malformed.
        kUnsafePath = 4,  ///< A manifest names a path outside the root.
        kBadConfiguration = 5, ///< A configuration file or setting is invalid.
        kUnresolvable = 6,     ///< No set of packages meets every relation.
    };

    /// A recoverable failure: its code and a message for the user.
    struct Error
    {
        ErrorCode code;
        std::string message;
    };

    /// A value, or the error that kept it from being made.
    template<typename Value>
    using Expected = tl::expected<Value, Error>;

    /// Makes the unexpected result of an operation that failed.
    ///
    /// @param code    The error code.
    /// @param message What went wrong, for the user.
    /// @return The result to return.
    inline tl::unexpected<Error>
    Fail(ErrorCode code, std::string message)
    {
        return tl::unexpected<Error>(Error{ code, std::move(message) });
    }

    /// The code of an error as the user sees it, such as `TPM0003`.
    ///
    /// @param code The error code.
    /// @return The text.
    std::string
    CodeName(ErrorCode code);

    /// What an error code means, for `talapm explain`.
    ///
    /// @param code The error code, as `TPM0003` or `3`.
    /// @return The explanation, or an error when no such code exists.
    Expected<std::string_view>
    Explain(std::string_view code);

} // namespace Progmasoft::TalaPM
