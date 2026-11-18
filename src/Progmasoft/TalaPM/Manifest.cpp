// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MIT

#include <array>
#include <charconv>
#include <climits>
#include <cstring>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <memory>
#include <system_error>
#include <utility>

#include "Progmasoft/TalaPM/Manifest.hpp"

namespace Progmasoft::TalaPM
{
    namespace
    {

        struct DocumentDeleter
        {
            void
            operator()(xmlDoc *document) const
            {
                xmlFreeDoc(document);
            }
        };

        using Document = std::unique_ptr<xmlDoc, DocumentDeleter>;

        /// The name of an element or attribute, as text.
        std::string_view
        NameOf(const xmlNode *node)
        {
            return reinterpret_cast<const char *>(node->name);
        }

        /// The value of an attribute, when it is there.
        std::optional<std::string>
        Attribute(const xmlNode *node, const char *name)
        {
            xmlChar *value
                = xmlGetProp(node, reinterpret_cast<const xmlChar *>(name));
            if (value == nullptr)
            {
                return std::nullopt;
            }
            std::string text = reinterpret_cast<const char *>(value);
            xmlFree(value);
            return text;
        }

        /// The text inside an element.
        std::string
        TextOf(const xmlNode *node)
        {
            xmlChar *content = xmlNodeGetContent(node);
            std::string text = content != nullptr
                                   ? reinterpret_cast<const char *>(content)
                                   : "";
            xmlFree(content);
            return text;
        }

        /// The child elements of an element.
        std::vector<const xmlNode *>
        Children(const xmlNode *node)
        {
            std::vector<const xmlNode *> children;
            for (const xmlNode *child = node->children; child != nullptr;
                 child = child->next)
            {
                if (child->type == XML_ELEMENT_NODE)
                {
                    children.push_back(child);
                }
            }
            return children;
        }

        tl::unexpected<Error>
        Malformed(std::string message)
        {
            return Fail(ErrorCode::kBadManifest,
                        "manifest: " + std::move(message));
        }

        /// Reads an unsigned decimal number, or an octal one for a mode.
        template<typename Number>
        std::optional<Number>
        ReadNumber(std::string_view text, int base)
        {
            Number value{};
            auto [end, error] = std::from_chars(text.data(),
                                                text.data() + text.size(),
                                                value,
                                                base);
            if (text.empty() || error != std::errc()
                || end != text.data() + text.size())
            {
                return std::nullopt;
            }
            return value;
        }

        /// Whether a path stays under the root: relative, with no empty, `.` or
        /// `..` component.
        bool
        IsSafePath(std::string_view path)
        {
            if (path.empty() || path.front() == '/')
            {
                return false;
            }
            std::size_t start = 0;
            while (start <= path.size())
            {
                std::size_t end = path.find('/', start);
                end = end == std::string_view::npos ? path.size() : end;
                std::string_view component = path.substr(start, end - start);
                if (component.empty() || component == "." || component == "..")
                {
                    return false;
                }
                start = end + 1;
            }
            return path.find('\0') == std::string_view::npos;
        }

        bool
        IsSha256(std::string_view text)
        {
            if (text.size() != 64)
            {
                return false;
            }
            for (char character : text)
            {
                bool isHex = (character >= '0' && character <= '9')
                             || (character >= 'a' && character <= 'f');
                if (!isHex)
                {
                    return false;
                }
            }
            return true;
        }

        /// The fields of `<package>`.
        struct PackageFields
        {
            std::string name;
            std::string version;
            std::string release;
            std::string architecture;
            std::string summary;
            std::string description;
            std::string license;
            std::string homepage;
            std::string packager;
            std::string built;
            std::string installedSize;
        };

        Expected<PackageFields>
        ReadPackageFields(const xmlNode *package)
        {
            PackageFields fields;
            const std::array<std::pair<const char *, std::string *>, 11>
                kFields{ {
                    { "name", &fields.name },
                    { "version", &fields.version },
                    { "release", &fields.release },
                    { "architecture", &fields.architecture },
                    { "summary", &fields.summary },
                    { "description", &fields.description },
                    { "license", &fields.license },
                    { "homepage", &fields.homepage },
                    { "packager", &fields.packager },
                    { "built", &fields.built },
                    { "installed-size", &fields.installedSize },
                } };
            std::array<bool, kFields.size()> seen{};
            for (const xmlNode *child : Children(package))
            {
                std::size_t index = 0;
                while (index < kFields.size()
                       && NameOf(child) != kFields[index].first)
                {
                    ++index;
                }
                if (index == kFields.size())
                {
                    return Malformed("unknown element <"
                                     + std::string(NameOf(child))
                                     + "> in <package>");
                }
                if (seen[index])
                {
                    return Malformed("<" + std::string(NameOf(child))
                                     + "> appears twice");
                }
                seen[index] = true;
                *kFields[index].second = TextOf(child);
            }
            for (std::size_t index = 0; index < kFields.size(); ++index)
            {
                // The description and the homepage may be left out.
                bool isOptional = index == 5 || index == 7;
                if (!seen[index] && !isOptional)
                {
                    return Malformed("<package> has no <"
                                     + std::string(kFields[index].first) + ">");
                }
            }
            return fields;
        }

        Expected<Architecture>
        ReadArchitecture(std::string_view text)
        {
            for (Architecture architecture : { Architecture::kAmd64,
                                               Architecture::kAarch64,
                                               Architecture::kNoarch })
            {
                if (text == ArchitectureName(architecture))
                {
                    return architecture;
                }
            }
            return Malformed("architecture '" + std::string(text)
                             + "' is not amd64, aarch64 or noarch");
        }

        Expected<void>
        ReadPackage(const xmlNode *package, Manifest &manifest)
        {
            Expected<PackageFields> fields = ReadPackageFields(package);
            if (!fields)
            {
                return tl::unexpected(fields.error());
            }
            if (!IsPackageName(fields->name))
            {
                return Fail(ErrorCode::kBadManifest,
                            "manifest: '" + fields->name
                                + "' is not a valid package name");
            }
            Expected<PackageVersion> version = PackageVersion::Parse(
                fields->version + "-" + fields->release);
            Expected<Architecture> architecture
                = ReadArchitecture(fields->architecture);
            std::optional<std::uint64_t> size
                = ReadNumber<std::uint64_t>(fields->installedSize, 10);
            if (!version)
            {
                return tl::unexpected(version.error());
            }
            if (!architecture)
            {
                return tl::unexpected(architecture.error());
            }
            if (!size)
            {
                return Fail(
                    ErrorCode::kBadManifest,
                    "manifest: installed-size is not a number of bytes");
            }
            manifest.name = std::move(fields->name);
            manifest.version = std::move(*version);
            manifest.architecture = *architecture;
            manifest.summary = std::move(fields->summary);
            manifest.description = std::move(fields->description);
            manifest.license = std::move(fields->license);
            manifest.homepage = std::move(fields->homepage);
            manifest.packager = std::move(fields->packager);
            manifest.built = std::move(fields->built);
            manifest.installedSizeBytes = *size;
            return {};
        }

        Expected<std::optional<Version>>
        ReadBound(const xmlNode *node, const char *name)
        {
            std::optional<std::string> text = Attribute(node, name);
            if (!text)
            {
                return std::optional<Version>();
            }
            Expected<Version> version = Version::Parse(*text);
            if (!version)
            {
                return tl::unexpected(version.error());
            }
            return std::optional<Version>(std::move(*version));
        }

        Expected<Relation>
        ReadRelation(const xmlNode *node)
        {
            constexpr std::array<std::pair<std::string_view, RelationKind>, 5>
                kKinds{ {
                    { "depends", RelationKind::kDepends },
                    { "recommends", RelationKind::kRecommends },
                    { "conflicts", RelationKind::kConflicts },
                    { "provides", RelationKind::kProvides },
                    { "replaces", RelationKind::kReplaces },
                } };
            std::optional<RelationKind> kind;
            for (const auto &[name, known] : kKinds)
            {
                kind = NameOf(node) == name ? std::optional(known) : kind;
            }
            std::optional<std::string> name = Attribute(node, "name");
            if (!kind)
            {
                return Malformed("unknown relation <"
                                 + std::string(NameOf(node)) + ">");
            }
            if (!name || !IsPackageName(*name))
            {
                return Malformed("a relation names no valid package");
            }
            Expected<std::optional<Version>> atLeast
                = ReadBound(node, "at-least");
            Expected<std::optional<Version>> below = ReadBound(node, "below");
            if (!atLeast || !below)
            {
                return tl::unexpected(!atLeast ? atLeast.error()
                                               : below.error());
            }
            return Relation{ *kind,
                             std::move(*name),
                             std::move(*atLeast),
                             std::move(*below) };
        }

        Expected<std::uint32_t>
        ReadMode(const xmlNode *node)
        {
            std::optional<std::string> text = Attribute(node, "mode");
            std::optional<std::uint32_t> mode
                = text ? ReadNumber<std::uint32_t>(*text, 8) : std::nullopt;
            // The permission bits with set-user-ID, set-group-ID and sticky.
            if (!mode || *mode > 07777)
            {
                return Malformed("an entry has no valid octal mode");
            }
            return *mode;
        }

        /// Reads what an entry of `<files>` has beyond its path.
        Expected<void>
        ReadEntryDetails(const xmlNode *node, Entry &entry)
        {
            if (entry.kind == EntryKind::kSymlink)
            {
                std::optional<std::string> target = Attribute(node, "target");
                if (!target || target->empty())
                {
                    return Fail(ErrorCode::kBadManifest,
                                "manifest: symlink " + entry.path
                                    + " has no target");
                }
                entry.target = std::move(*target);
                return {};
            }
            Expected<std::uint32_t> mode = ReadMode(node);
            if (!mode)
            {
                return tl::unexpected(mode.error());
            }
            entry.mode = *mode;
            entry.owner = Attribute(node, "owner").value_or("root");
            entry.group = Attribute(node, "group").value_or("wheel");
            if (entry.kind == EntryKind::kDirectory)
            {
                return {};
            }
            std::optional<std::string> size = Attribute(node, "size");
            std::optional<std::uint64_t> bytes
                = size ? ReadNumber<std::uint64_t>(*size, 10) : std::nullopt;
            entry.sha256 = Attribute(node, "sha256").value_or("");
            entry.isConfiguration
                = Attribute(node, "config").value_or("") == "true";
            if (!bytes || !IsSha256(entry.sha256))
            {
                return Fail(ErrorCode::kBadManifest,
                            "manifest: file " + entry.path
                                + " needs a size and a lowercase SHA-256");
            }
            entry.sizeBytes = *bytes;
            return {};
        }

        Expected<Entry>
        ReadEntry(const xmlNode *node)
        {
            constexpr std::array<std::pair<std::string_view, EntryKind>, 3>
                kKinds{ {
                    { "directory", EntryKind::kDirectory },
                    { "file", EntryKind::kFile },
                    { "symlink", EntryKind::kSymlink },
                } };
            std::optional<EntryKind> kind;
            for (const auto &[name, known] : kKinds)
            {
                kind = NameOf(node) == name ? std::optional(known) : kind;
            }
            if (!kind)
            {
                return Malformed("unknown entry <" + std::string(NameOf(node))
                                 + ">");
            }
            Entry entry;
            entry.kind = *kind;
            entry.path = Attribute(node, "path").value_or("");
            if (!IsSafePath(entry.path))
            {
                return Fail(ErrorCode::kUnsafePath,
                            "manifest: path '" + entry.path
                                + "' leaves the root");
            }
            Expected<void> details = ReadEntryDetails(node, entry);
            if (!details)
            {
                return tl::unexpected(details.error());
            }
            return entry;
        }

        Expected<Script>
        ReadScript(const xmlNode *node)
        {
            constexpr std::array<std::pair<std::string_view, ScriptPhase>, 6>
                kPhases{ {
                    { "pre-install", ScriptPhase::kPreInstall },
                    { "post-install", ScriptPhase::kPostInstall },
                    { "pre-upgrade", ScriptPhase::kPreUpgrade },
                    { "post-upgrade", ScriptPhase::kPostUpgrade },
                    { "pre-remove", ScriptPhase::kPreRemove },
                    { "post-remove", ScriptPhase::kPostRemove },
                } };
            std::string phaseText = Attribute(node, "phase").value_or("");
            std::string path = Attribute(node, "path").value_or("");
            std::optional<ScriptPhase> phase;
            for (const auto &[name, known] : kPhases)
            {
                phase = phaseText == name ? std::optional(known) : phase;
            }
            if (NameOf(node) != "script" || !phase)
            {
                return Malformed("a script has no known phase");
            }
            constexpr std::string_view kScripts = ".META/SCRIPTS/";
            if (!IsSafePath(path)
                || path.compare(0, kScripts.size(), kScripts) != 0)
            {
                return Fail(ErrorCode::kUnsafePath,
                            "manifest: script '" + path + "' is not under "
                                + std::string(kScripts));
            }
            return Script{ *phase, std::move(path) };
        }

        /// Reads every child of a list element with one reader.
        template<typename Item, typename Reader>
        Expected<void>
        ReadList(const xmlNode *list, std::vector<Item> &items, Reader read)
        {
            for (const xmlNode *child : Children(list))
            {
                Expected<Item> item = read(child);
                if (!item)
                {
                    return tl::unexpected(item.error());
                }
                items.push_back(std::move(*item));
            }
            return {};
        }

        /// Reads the sections of `<manifest>` into a manifest.
        Expected<void>
        ReadSections(const xmlNode *root, Manifest &manifest)
        {
            bool hasPackage = false;
            for (const xmlNode *section : Children(root))
            {
                std::string_view name = NameOf(section);
                Expected<void> result;
                if (name == "package" && !hasPackage)
                {
                    hasPackage = true;
                    result = ReadPackage(section, manifest);
                }
                else if (name == "relations")
                {
                    result
                        = ReadList(section, manifest.relations, ReadRelation);
                }
                else if (name == "files")
                {
                    result = ReadList(section, manifest.entries, ReadEntry);
                }
                else if (name == "scripts")
                {
                    result = ReadList(section, manifest.scripts, ReadScript);
                }
                else
                {
                    result = Fail(ErrorCode::kBadManifest,
                                  "manifest: unexpected <" + std::string(name)
                                      + ">");
                }
                if (!result)
                {
                    return result;
                }
            }
            if (!hasPackage)
            {
                return Fail(ErrorCode::kBadManifest, "manifest: no <package>");
            }
            return {};
        }

    } // namespace

    bool
    Relation::Admits(const Version &version) const
    {
        return (!atLeast || version >= *atLeast)
               && (!below || version < *below);
    }

    bool
    IsPackageName(std::string_view name)
    {
        if (name.empty())
        {
            return false;
        }
        for (std::size_t index = 0; index < name.size(); ++index)
        {
            char character = name[index];
            bool isAlphanumeric = (character >= 'a' && character <= 'z')
                                  || (character >= '0' && character <= '9');
            bool isMark = std::strchr("+-._", character) != nullptr
                          && character != '\0' && index > 0;
            if (!isAlphanumeric && !isMark)
            {
                return false;
            }
        }
        return true;
    }

    std::string_view
    ArchitectureName(Architecture architecture)
    {
        switch (architecture)
        {
            case Architecture::kAmd64:
                return "amd64";
            case Architecture::kAarch64:
                return "aarch64";
            case Architecture::kNoarch:
                return "noarch";
        }
        return "noarch";
    }

    Expected<Manifest>
    ParseManifest(std::string_view xml)
    {
        if (xml.size() > static_cast<std::size_t>(INT_MAX))
        {
            return Malformed("the manifest is too large");
        }
        // No network, no DTD loaded, no entity expanded: a manifest is data.
        constexpr int kOptions = XML_PARSE_NONET | XML_PARSE_NOERROR
                                 | XML_PARSE_NOWARNING | XML_PARSE_NO_XXE;
        Document document(xmlReadMemory(xml.data(),
                                        static_cast<int>(xml.size()),
                                        "MANIFEST.XML",
                                        nullptr,
                                        kOptions));
        if (document == nullptr)
        {
            return Malformed("not well-formed XML");
        }
        if (document->intSubset != nullptr || document->extSubset != nullptr)
        {
            return Malformed("a document type declaration is not allowed");
        }
        const xmlNode *root = xmlDocGetRootElement(document.get());
        if (root == nullptr || NameOf(root) != "manifest")
        {
            return Malformed("the root element is not <manifest>");
        }
        std::optional<std::string> format = Attribute(root, "format");
        if (!format || *format != std::to_string(kManifestFormat))
        {
            return Malformed("format '" + format.value_or("")
                             + "' is not one this TalaPM reads (1)");
        }
        Manifest manifest{};
        Expected<void> sections = ReadSections(root, manifest);
        if (!sections)
        {
            return tl::unexpected(sections.error());
        }
        return manifest;
    }

} // namespace Progmasoft::TalaPM
