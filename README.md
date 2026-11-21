<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MIT -->

# TalaPM

TalaPM is the package manager of TalaOS. It installs `.talapkg` packages from the TalaOS repositories, resolves their
dependencies with a SAT solver, and supports partial upgrades.

Every change is atomic: an install, an upgrade or a removal either completes or leaves the system as it was. TalaPM
unpacks and checks the new files beside the old ones, records the transaction in a journal, then swaps them into
place with renames; after a crash or a power cut, the journal finishes the transaction or rolls it back.

The commands:

| Command | What it does |
| --- | --- |
| `install <package>` | Installs the package; reinstalls it if it is installed |
| `install <package> --update`, `-u` | Synchronizes the database, then installs the package |
| `install <package> --upgrade`, `-U` | Upgrades the system packages, then installs the package |
| `install <package> --needs` | Installs only what is needed |
| `install <package> --with-recommendeds` | Installs or reinstalls the package and installs its recommended packages that are not installed |
| `remove <package>` | Removes the package |
| `purge <package>` | Removes the package with its configuration and cache files |
| `update` | Synchronizes the database |
| `upgrade` | Upgrades the system packages |
| `upgrade --check` | Lists the packages that can be upgraded, without upgrading |
| `upgrade <package>` | Upgrades one system package |
| `upgrade --update`, `upgrade -u`, `update --upgrade`, `update -U` | Synchronizes the database and upgrades the system packages |
| `full-upgrade` | Upgrades the system packages, removing packages where needed |
| `full-upgrade --update`, `-u` | Synchronizes the database, then does a full upgrade |
| `auto-remove` | Removes dependencies that nothing needs |
| `search <package>` | Searches for packages |
| `info <package>` | Shows a package's information |
| `list --installed`, `--available`, `--upgradable` | Lists packages |
| `list <package> --versions`, `--depends`, `--rdepends` | Shows a package's versions, dependencies or reverse dependencies |
| `policy <package>` | Shows a package's versions and where they come from |
| `provides <file>` | Finds the package that provides a file |
| `cache clean all`, `packages`, `meta` | Cleans the whole cache, the downloaded packages, or the repository metadata |
| `cache make` | Creates the repository metadata; fails if it exists |
| `cache make --force`, `cache make!` | Creates the repository metadata, overwriting it if it exists |
| `repo list`, `--enabled`, `--disabled` | Lists the repositories |
| `explain <error-code>` | Explains an error |

`-s` or `--setting` overrides the configuration for one command, in the syntax of the configuration files:
`-s '<toml>'`, `-s'<toml>'`, `--setting '<toml>'` and `--setting='<toml>'`.

## Options of every command

They stand anywhere on the command line, before or after the command:

| Option | Meaning |
| --- | --- |
| `--help`, `-h` | The help of TalaPM, or of the command it follows: `talapm install --help` |
| `--version` | The version |
| `-y`, `--yes` | Answer yes to every question |
| `--dry-run` | Show what would be done, and do nothing |
| `-q`, `--quiet` | Write nothing but problems: the verbosity `"Always-Silent"` |
| `-V`, `--verbose` | Write everything: the verbosity `"Always-Verbose"`; not with `-q` |
| `--color always\|auto\|never`, `--color=…` | Colors, over `[color] level` |
| `--root <path>`, `--root=<path>` | Act on the system at that root: its `etc/` and its keyrings |
| `--default-config` | Read no configuration file: TalaPM's defaults, and `-s` on top |

`-q`, `-V` and `--color` win over every configuration source, `-s` included.

## The config command

| Command | Prints |
| --- | --- |
| `config dump-default` | The default configuration, as TOML |
| `config dump` | The configuration in effect, as TOML: every key, the repositories in their priority |
| `config get <setting>` | One setting of it as a TOML value: `config get download.parallel` prints `4` |
| `config validate` | Nothing; it fails naming the first problem, a missing keyring of an enabled repository too |

## Configuration

The configuration is TOML, in files that all have the same syntax. From the highest to the lowest, each overriding
those below it:

1. `-s` and `--setting` on the command line, a later one over an earlier one.
2. Files in `/etc/talapm.d/`, at any depth, read in the order of the bytes of their names (then of their paths), a
   later one over an earlier one: `90-local.toml` overrides `10-vendor.toml`, as in `sysctl.d` and `conf.d`.
3. `/etc/talapm.conf.toml`, which [etc/talapm.conf.toml](etc/talapm.conf.toml) installs with the defaults.
4. `/etc/talapm.conf.d/config.toml`, installed empty.

A file in `/etc/talapm.d/` counts only when its name ends in `.toml`, so `.conf.toml` too; `.toml.conf`, `.yaml`,
`.json`, `.ini`, a name without an extension and every other file are not read at all. That directory is installed
empty. A key TalaPM does not know is an error that names its file, and so is a mirror that is
not `https://`. Repositories and mirrors keep the order they are written in: the first is tried first.

`download.verbosity` is `"Always-Verbose"`, `"Somewhat-Verbose"`, `"User-Friendly"` (the default), `"Near-Silent"` or
`"Always-Silent"`; `[color] level` is `"Auto"` (the default), `"Always"` or `"Never"`.

The built-in repositories, `[repo.built-in.core]` and the others, take only `enabled`: their mirrors and keyrings are
built into TalaPM. A repository of your own is a table beside them, with its mirrors, and its keyring in
`/etc/talapm.keyrings/repo/<name>.gpg`:

```toml
[repo.example]
enabled = true
mirror-uris = ["https://repo.example.org/"]
```

## Packages

A `.talapkg` is a tar archive compressed with xz. Beside the files it installs, it holds `.META/MANIFEST.XML`, which
describes the package:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<manifest format="1">
  <package>
    <name>tala-utils</name>
    <version>1.0.0</version>
    <release>1</release>
    <architecture>amd64</architecture>
    <summary>BSD utilities and tools such as sudo</summary>
    <description>The base utilities of TalaOS.</description>
    <license>MIT</license>
    <homepage>https://github.com/Progmasoft/tala-utils</homepage>
    <packager>Progmasoft &lt;support@progmasoft.com&gt;</packager>
    <built>2027-05-01T12:00:00Z</built>
    <installed-size>1843200</installed-size>
  </package>

  <relations>
    <depends name="opencrt" at-least="1.0.0" below="2.0.0"/>
  </relations>

  <files>
    <directory path="usr/share/tala-utils" mode="0755" owner="root" group="wheel"/>
    <file path="usr/bin/ls" mode="0755" owner="root" group="wheel" size="81920"
          sha256="9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"/>
    <file path="usr/bin/sudo" mode="4755" owner="root" group="wheel" size="163840"
          sha256="60303ae22b998861bce3b28f33eec1be758a213c86c93c076dbe9f558c11c752"/>
    <file path="etc/sudoers" mode="0440" owner="root" group="wheel" size="1024"
          sha256="fd61a03af4f77d870fc21e05e7e80678095c92d808cfb3b5c279ee04c74aca13" config="true"/>
    <symlink path="usr/bin/[" target="test"/>
  </files>

  <scripts>
    <script phase="post-install" path=".META/SCRIPTS/post-install"/>
  </scripts>
</manifest>
```

- **`format`** is the version of the manifest format.
- **`<package>`:** `version` is the software's version and `release` counts the packagings of that version; the
  `$version` of a file name joins them (`1.0.0-1`). `architecture` is `amd64`, `aarch64` or `noarch`; `license` is an SPDX
  expression; `installed-size` is in bytes and is what `check-space` checks. The manifest does not name a repository:
  a package belongs to the repository it is published in.
- **`<relations>`** holds `depends`, `recommends` (what `--with-recommendeds` and `weak-deps` install), `conflicts`,
  `provides` (a virtual name) and `replaces`. Each names a package, and may bound its version with `at-least`
  (inclusive) and `below` (exclusive).
- **`<files>`** lists each directory, file and symbolic link with its path from the root, and each file with its mode,
  owner, group, size and SHA-256. `provides <file>` searches these lists. `remove` keeps a file marked
  `config="true"`; `purge` removes it.
- **`<scripts>`** are optional hooks of the install and remove phases, stored under `.META/SCRIPTS/`.
- The signatures of the package and the database are not part of the manifest; the `[signature]` settings govern them.

It is written in C++17 under the
[Progmasoft Core C++ Coding Guidelines](https://github.com/Progmasoft/progmasoft-cpp) and built with Bazel. It uses
CaDiCaL, libwebsockets, GnuTLS, toml++, yyjson, libarchive, liblzma, libxml2, GPGME++, tl::expected, fmt, indicators
and tabulate, and is tested
with [Catch3](https://github.com/Progmasoft/catch3) and ankerl::nanobench. It talks to repositories over HTTPS:
libwebsockets is its HTTP client and GnuTLS its TLS; it uses no WebSocket.

The parts that need no operating system are written: the command line, read by hand into typed commands; versions
and their order; the manifest reader, which reads XML without a network, a DTD or entities and refuses any path that
would leave the root; and the configuration, applied from its lowest source to `-s`. Downloading, unpacking,
installing and the resolver come next.

```sh
bazelisk test //...                # the tests
bazelisk test --config=asan //...  # with AddressSanitizer and UndefinedBehaviorSanitizer
```

## License

[MIT](LICENSE.txt), also covered by the [Progmasoft Patent Grant 1.1](PATENTS.md). Participation follows the
[Code of Conduct](CODE_OF_CONDUCT.md).
