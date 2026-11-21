<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MIT -->

# Changelog

Versions follow [Semantic Versioning](https://semver.org/).

## Unreleased

- The files of `etc/talapm.d/` are read in the byte order of their names, and a later one overrides an earlier one, so
  `90-local.toml` is above `10-vendor.toml`. A file whose name started with a character other than a letter or a digit,
  or with a digit, came first and overrode the others.

- The repository of TalaPM: README, MIT license, Patent Grant and code of conduct.
- The package format: a `.talapkg` is tar + xz with `.META/MANIFEST.XML`.
