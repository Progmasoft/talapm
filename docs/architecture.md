<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MIT -->

# Architecture

The parts of TalaPM that need no operating system are the library `//:core`, in `src/Progmasoft/TalaPM/`, namespace
`Progmasoft::TalaPM`. Each module returns `tl::expected` values; there are no exceptions.

| Module | What it does |
| --- | --- |
| `Error` | The error codes `TPM0001`–`TPM0006` and the text of `talapm explain`. |
| `CommandLine` | The command line, read by hand into typed commands. |
| `Version` | Versions and their order; package versions with their release. |
| `Manifest` | `.META/MANIFEST.XML`, read with libxml2 without a network, a DTD or entities; paths kept under the root. |
| `Configuration` | The configuration from its sources, applied from the lowest to `-s`; drop-ins ordered by name. |
| `ConfigurationFiles` | Reading those sources from a system's `etc/`, `.toml` files only, none over 1 MiB. |

`etc/` holds the files TalaPM installs: `talapm.conf.toml` with the defaults and an empty `talapm.conf.d/config.toml`.
`third_party/` builds indicators and tabulate, which are not in the Bazel Central Registry.
