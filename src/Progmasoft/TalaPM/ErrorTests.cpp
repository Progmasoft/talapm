// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MIT

#include <Progmasoft/Catch3/All.hpp>

#include "Progmasoft/TalaPM/Error.hpp"

using Progmasoft::TalaPM::CodeName;
using Progmasoft::TalaPM::ErrorCode;
using Progmasoft::TalaPM::Explain;

TEST_CASE("error codes have names and explanations")
{
    CHECK(CodeName(ErrorCode::kUsage) == "TPM0001");
    CHECK(CodeName(ErrorCode::kUnresolvable) == "TPM0006");
    REQUIRE(Explain("TPM0001").has_value());
    CHECK(Explain("TPM0001")->find("command line") != std::string::npos);
    CHECK(Explain("3").value() == Explain("TPM0003").value());
    CHECK_FALSE(Explain("TPM0099").has_value());
    CHECK_FALSE(Explain("").has_value());
}
