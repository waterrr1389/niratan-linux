// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QFile>
#include <QString>

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <utility>
#include <vector>

namespace niratan::test {

// Writes a stored (uncompressed) zip. The test must not depend on a zip
// encoder, and hoshidicts reads stored entries fine.
inline void write_stored_zip(const std::filesystem::path& path,
                             const std::vector<std::pair<QString, QByteArray>>& entries) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::WriteOnly)) {
        throw std::runtime_error("cannot create fixture zip");
    }

    struct Local {
        quint32 offset = 0;
        quint32 crc = 0;
        quint32 size = 0;
        QByteArray name;
    };
    std::vector<Local> locals;
    locals.reserve(entries.size());

    auto put16 = [](unsigned char* out, int at, quint16 value) {
        out[at] = static_cast<unsigned char>(value & 0xff);
        out[at + 1] = static_cast<unsigned char>((value >> 8) & 0xff);
    };
    auto put32 = [](unsigned char* out, int at, quint32 value) {
        for (int i = 0; i < 4; ++i) {
            out[at + i] = static_cast<unsigned char>((value >> (8 * i)) & 0xff);
        }
    };

    for (const auto& [name, data] : entries) {
        Local local;
        local.offset = static_cast<quint32>(file.pos());
        local.crc = static_cast<quint32>(qChecksum(data));
        local.size = static_cast<quint32>(data.size());
        local.name = name.toUtf8();

        // Local file header layout is fixed by the ZIP specification:
        // signature, version, flags, method, time, date, crc, sizes, then
        // the name length. A shifted field makes readers reject the entry.
        QByteArray header(30, '\0');
        auto* out = reinterpret_cast<unsigned char*>(header.data());
        put32(out, 0, 0x04034b50);
        put16(out, 4, 20);
        put16(out, 26, static_cast<quint16>(local.name.size()));
        put32(out, 14, local.crc);
        put32(out, 18, local.size);
        put32(out, 22, local.size);
        file.write(header);
        file.write(local.name);
        file.write(data);
        locals.push_back(local);
    }

    const auto central_offset = static_cast<quint32>(file.pos());
    for (const auto& local : locals) {
        QByteArray header(46, '\0');
        auto* out = reinterpret_cast<unsigned char*>(header.data());
        put32(out, 0, 0x02014b50);
        put16(out, 4, 20);
        put16(out, 6, 20);
        put16(out, 28, static_cast<quint16>(local.name.size()));
        put32(out, 16, local.crc);
        put32(out, 20, local.size);
        put32(out, 24, local.size);
        put32(out, 42, local.offset);
        file.write(header);
        file.write(local.name);
    }
    const auto central_size = static_cast<quint32>(file.pos()) - central_offset;

    QByteArray end(22, '\0');
    auto* out = reinterpret_cast<unsigned char*>(end.data());
    put32(out, 0, 0x06054b50);
    put16(out, 8, static_cast<quint16>(locals.size()));
    put16(out, 10, static_cast<quint16>(locals.size()));
    put32(out, 12, central_size);
    put32(out, 16, central_offset);
    file.write(end);
}

// Field order follows glz::meta<Term> in src/json/yomitan_parser.cpp. "v"
// marks a verb so the library can deinflect "decided" back to "decide".
inline QByteArray minimal_term_bank() {
    return "[[\"decide\",\"\",\"\",\"v\",0,[\"to make up one's mind\"],1,\"\"]]";
}

// glz::meta<Index> requires every field; omitted keys fail the parse.
inline QByteArray minimal_index(const QString& title) {
    return QString(
               "{\"title\":\"%1\",\"revision\":\"1\",\"format\":3,"
               "\"isUpdatable\":false,\"indexUrl\":\"\",\"downloadUrl\":\"\"}")
        .arg(title)
        .toUtf8();
}

}  // namespace niratan::test
