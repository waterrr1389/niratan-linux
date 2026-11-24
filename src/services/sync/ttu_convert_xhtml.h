// SPDX-License-Identifier: GPL-3.0-or-later
// TtuConverter.swift: StaticData, Section, XHTMLFile and document templates.

#pragma once

#include <QString>

#include <optional>
#include <vector>

namespace niratan::sync::detail {

struct Section {
    QString reference;
    qint64 weight = 0;
    std::optional<QString> label;
    std::optional<qint64> start;
    std::optional<qint64> characters;
    std::optional<QString> parent;
};

struct XhtmlFile {
    QString name;
    std::optional<QString> label;
    QString slice;
};

QString export_element(QString reference, const QString& chapter, const QString& manifest_path,
                       bool no_text);
std::vector<XhtmlFile> split_elements(QString html, const std::vector<Section>& sections);
QString xhtml(const XhtmlFile& file, const QString& title);
QString navigation(const std::vector<XhtmlFile>& files);
QString opf(const QStringList& images, const std::vector<XhtmlFile>& files, const QString& title,
            const QString& uuid);

}  // namespace niratan::sync::detail
