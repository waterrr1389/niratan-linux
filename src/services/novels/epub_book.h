// SPDX-License-Identifier: GPL-3.0-or-later
//
// An opened EPUB container, following the metadata, spine and table of
// contents that Core/BookStorage.swift and the reader consume through
// EPUBKit, using the system libzip (SPEC.md 3.5).
//
// open() reads META-INF/container.xml, the first rootfile package, the OPF
// metadata, manifest, spine and guide, then the table of contents. The EPUB 3
// nav document (a manifest item with the "nav" property) wins; without one the
// EPUB 2 NCX named by the spine's toc attribute is used. TOC entries are
// flattened in document order with a zero based nesting level; nav links are
// leveled by their <ol> depth and navPoints by their nesting depth.
//
// A spine item keeps the manifest item's inputs as written (raw href and
// properties) beside the resolved href, and the linear attribute's reading
// ((item.attributes["linear"] ?? "yes") == "yes"). The guide keeps the direct
// <guide><reference> children in document order, with raw type and href and no
// title.
//
// A repeated manifest id resolves to the first item, and references from every
// <guide> element are kept; Swift's manifest keeps the last item for a repeated
// id and reads only the first guide. EPUB requires unique ids and a single
// guide, so valid packages are unaffected.
//
// Metadata keeps the first dc:title and dc:creator text as written. The
// cover is the manifest item with the "cover-image" property, else the item
// named by <meta name="cover">. Hrefs in the results are container paths
// (relative to the zip root), percent-decoded once and resolved against the
// package directory; a path that climbs out of the container is rejected.
//
// read() returns an entry's bytes, or nullopt when the entry is absent. The
// caller owns the returned HTML; this layer does not rewrite it.

#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace niratan::novels {

struct EpubMetadata {
    std::string title;
    std::optional<std::string> author;
    // dc:language, as written; the library stores it on the book.
    std::optional<std::string> language;
    std::optional<std::string> cover_href;

    bool operator==(const EpubMetadata&) const = default;
};

struct EpubManifestItem {
    std::string id;
    std::string href;
    std::string path;
    std::string media_type;
    std::string properties;

    bool operator==(const EpubManifestItem&) const = default;
};

struct EpubSpineItem {
    std::string idref;
    std::string href;
    std::string media_type;
    // The manifest item's raw href attribute, exactly as written: not
    // percent-decoded and not resolved against the package directory.
    std::string manifest_href;
    // The manifest item's raw properties attribute, empty when absent.
    std::string properties;
    // (item.attributes["linear"] ?? "yes") == "yes"
    bool linear = true;

    bool operator==(const EpubSpineItem&) const = default;
};

// A direct <guide><reference> child: the raw type and href attributes. A
// reference missing either attribute is not represented, and title is not
// exposed.
struct EpubGuideReference {
    std::string type;
    std::string href;

    bool operator==(const EpubGuideReference&) const = default;
};

struct EpubTocEntry {
    // Zero for a top level entry, one for its children, and so on.
    int level = 0;
    std::string title;
    std::string href;

    bool operator==(const EpubTocEntry&) const = default;
};

class EpubBook {
   public:
    // nullopt with a reason in error when the file is not a readable EPUB.
    static std::optional<EpubBook> open(const std::filesystem::path& path,
                                        std::string* error = nullptr);

    EpubBook(EpubBook&& other) noexcept;
    EpubBook& operator=(EpubBook&& other) noexcept;
    ~EpubBook();

    EpubBook(const EpubBook&) = delete;
    EpubBook& operator=(const EpubBook&) = delete;

    const EpubMetadata& metadata() const;
    const std::vector<EpubManifestItem>& manifest() const;
    const std::vector<EpubSpineItem>& spine() const;
    const std::vector<EpubTocEntry>& toc() const;
    // Direct <guide><reference> children of the package document, in document
    // order, with the raw type and href attributes; an empty vector when the
    // package has no guide.
    const std::vector<EpubGuideReference>& guide() const;

    std::optional<std::string> read(std::string_view href) const;

   private:
    struct Impl;
    explicit EpubBook(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

}  // namespace niratan::novels
