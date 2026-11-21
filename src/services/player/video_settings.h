// SPDX-License-Identifier: GPL-3.0-or-later
//
// Core/UserConfig.swift (video preferences, init, resetVideoSubtitleAppearance),
// VideoSubtitlePositionPolicy.swift (legacy placement), VideoSubtitleEdgeStyle.swift
// (edge migration/recipe), and ColorHexCodec.swift (RGB/RGBA order).
// Linux keeps video preferences in settings.json. The Study width follows
// VideoPlayerScreen.studySidebarWidth and VideoMiningHistorySidebar's bounds:
// 340 by default, clamped to 320–560 and stored as an integer. Unknown keys
// are preserved; invalid documents are not replaced.

#pragma once

#include <QJsonObject>
#include <QString>

#include <filesystem>

namespace niratan::player {

QJsonObject video_settings(const QJsonObject& stored = {});
QJsonObject load_video_settings(const std::filesystem::path& directory);
bool store_video_settings(const std::filesystem::path& directory, const QJsonObject& changes);
QJsonObject subtitle_appearance_defaults();
QString mpv_color(const QString& rgba);

struct SubtitleEdges {
    double outline = 0;
    double shadow = 0;
};

SubtitleEdges subtitle_edges(const QString& style, double strength, double size);

}  // namespace niratan::player
