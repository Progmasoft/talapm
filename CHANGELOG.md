<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MIT -->

# Changelog

Versions follow [Semantic Versioning](https://semver.org/).

## Unreleased

- Options of every command: `--help` of TalaPM or of a command, `--version`, `-y`/`--yes`, `--dry-run`,
  `-q`/`--quiet`, `-V`/`--verbose`, `--color`, `--root` and `--default-config`. `-q`, `-V` and `--color` win over
  the configuration, `-s` included.
- `config dump-default`, `config validate`, `config dump` and `config get <setting>`.
- `[color] level`, `"Auto"`, `"Always"` or `"Never"`; `download.verbosity` takes `"Always-Verbose"`,
  `"Somewhat-Verbose"`, `"User-Friendly"`, `"Near-Silent"` and `"Always-Silent"`.
- Repositories of the user, `[repo.<name>]` with `mirror-uris` and a keyring in `etc/talapm.keyrings/repo/`; the
  built-in repositories take only `enabled`, their mirrors and keyrings being TalaPM's own.

- The repository of TalaPM: README, MIT license, Patent Grant and code of conduct.
- The package format: a `.talapkg` is tar + xz with `.META/MANIFEST.XML`.
