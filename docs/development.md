<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MIT -->

# Development

TalaPM is built with Bazel (Bazelisk reads `.bazelversion`).

```sh
bazelisk test //...                 # the Catch3 tests
bazelisk test --config=asan //...   # with AddressSanitizer and UndefinedBehaviorSanitizer
clang-format --dry-run --Werror src/Progmasoft/TalaPM/*
```

Each module has its tests beside it, in `<Module>Tests.cpp`. CI runs the tests, the same under the sanitizers,
clang-format 23.1.5, and zizmor and actionlint on the workflows.
