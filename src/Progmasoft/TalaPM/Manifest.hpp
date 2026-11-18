// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Progmasoft/TalaPM/Error.hpp"
#include "Progmasoft/TalaPM/Version.hpp"

namespace Progmasoft::TalaPM
{

    /// The architectures a package is built for.
    enum class Architecture
    {
        kAmd64,
        kAarch64,
        kNoarch,
    };

    /// The kinds of relation a package has to another.
    enum class RelationKind
    {
        kDepends,
        kRecommends,
        kConflicts,
        kProvides,
        kReplaces,
    };

    /// A relation to a package, with the versions it applies to: from `atLeast`
    /// (inclusive) to `below` (exclusive), each optional. The bounds compare
    /// the software's version, not the release.
    struct Relation
    {
        RelationKind kind;
        std::string name;
        std::optional<Version> atLeast;
        std::optional<Version> below;

        /// Whether a version of the package lies within the bounds.
        bool
        Admits(const Version &version) const;
    };

    /// What a file entry of a manifest is.
    enum class EntryKind
    {
        kDirectory,
        kFile,
        kSymlink,
    };

    /// A directory, a file or a symbolic link a package installs.
    struct Entry
    {
        EntryKind kind;
        /// The path from the root, relative, with no `.` or `..` component.
        std::string path;
        /// The permission bits, such as 04755; for a symbolic link, 0.
        std::uint32_t mode = 0;
        std::string owner;
        std::string group;
        /// For a file.
        std::uint64_t sizeBytes = 0;
        std::string sha256;
        /// For a file: kept by `remove`, removed by `purge`.
        bool isConfiguration = false;
        /// For a symbolic link.
        std::string target;
    };

    /// The phases a package script runs in.
    enum class ScriptPhase
    {
        kPreInstall,
        kPostInstall,
        kPreUpgrade,
        kPostUpgrade,
        kPreRemove,
        kPostRemove,
    };

    /// A script of a package, under `.META/SCRIPTS/`.
    struct Script
    {
        ScriptPhase phase;
        std::string path;
    };

    /// The `.META/MANIFEST.XML` of a package.
    struct Manifest
    {
        std::string name;
        PackageVersion version;
        Architecture architecture;
        std::string summary;
        std::string description;
        /// An SPDX license expression.
        std::string license;
        std::string homepage;
        std::string packager;
        /// When the package was built, in RFC 3339, such as
        /// 2027-05-01T12:00:00Z.
        std::string built;
        std::uint64_t installedSizeBytes;
        std::vector<Relation> relations;
        std::vector<Entry> entries;
        std::vector<Script> scripts;
    };

    /// The manifest format this TalaPM reads.
    constexpr int kManifestFormat = 1;

    /// Reads a manifest.
    ///
    /// The XML is read without a network, without a DTD and without expanding
    /// entities, so a manifest cannot make TalaPM read anything else. Every
    /// path stays under the root: an absolute path or a `..` component is
    /// kUnsafePath.
    ///
    /// @param xml The text of `.META/MANIFEST.XML`.
    /// @return The manifest, or kBadManifest or kUnsafePath.
    Expected<Manifest>
    ParseManifest(std::string_view xml);

    /// Whether a package name is valid: a lowercase letter or a digit, then
    /// lowercase letters, digits, `+`, `-`, `.` and `_`.
    ///
    /// @param name The name.
    /// @return true when it is.
    bool
    IsPackageName(std::string_view name);

    /// The name of an architecture, as manifests and file names write it.
    ///
    /// @param architecture The architecture.
    /// @return `amd64`, `aarch64` or `noarch`.
    std::string_view
    ArchitectureName(Architecture architecture);

} // namespace Progmasoft::TalaPM
