# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MIT

load("@rules_cc//cc:cc_library.bzl", "cc_library")

# tabulate, MIT, header-only.
cc_library(
    name = "tabulate",
    hdrs = glob(["include/tabulate/**/*.hpp"]),
    strip_include_prefix = "include",
    visibility = ["//visibility:public"],
)
