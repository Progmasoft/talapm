# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MIT

load("@rules_cc//cc:cc_library.bzl", "cc_library")

# indicators, MIT, header-only.
cc_library(
    name = "indicators",
    hdrs = glob(["include/indicators/**/*.hpp"]),
    strip_include_prefix = "include",
    visibility = ["//visibility:public"],
)
