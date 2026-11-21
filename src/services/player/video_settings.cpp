// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/player/video_settings.h"
#include "services/storage/json_file.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace niratan::player {
namespace {

struct Number {
    const char* key;
    double initial;
    double minimum;
    double maximum;
};

constexpr Number numbers[] = {
    {"videoSeekInterval", 5, 1, 60},
    {"videoMiningHistoryLimit", 25, 0, 1000},
    {"studySidebarWidth", 340, 320, 560},
    {"videoSubtitleGapFastForwardSpeed", 2.7, 1.1, 5},
    {"videoBrightness", 0, -100, 100},
    {"videoContrast", 0, -100, 100},
    {"videoSaturation", 0, -100, 100},
    {"videoGamma", 0, -100, 100},
    {"videoHue", 0, -100, 100},
    {"videoSubtitleFontSize", 36, 12, 72},
    {"videoSubtitleFontWeight", 700, 100, 900},
    {"videoSubtitleEdgeStrength", 0.5, 0, 1},
    {"videoSubtitleBackgroundOpacity", 0, 0, 1},
    {"videoSubtitleVerticalPositionFraction", 0.9, 0, 1},
    {"videoSubtitleMaskBlurRadius", 10, 0, 20},
    {"videoSubtitleMaskHiddenOpacity", 0, 0, 1},
};

QString color_hex(QString value, const QString& fallback) {
    static const QRegularExpression hex(QStringLiteral("\\A[0-9a-fA-F]+\\z"));
    value = value.trimmed();
    if (value.startsWith(u'#')) {
        value.remove(0, 1);
    }
    if (!hex.match(value).hasMatch()) {
        return fallback;
    }
    if (value.size() == 3 || value.size() == 4) {
        QString expanded;
        for (const QChar digit : value) {
            expanded += QString(2, digit);
        }
        value = expanded;
    }
    if (value.size() != 6 && value.size() != 8) {
        return fallback;
    }
    value = value.toUpper();
    if (value.size() == 8 && value.endsWith(QStringLiteral("FF"))) {
        value.chop(2);
    }
    return u'#' + value;
}

QString choice(const QJsonValue& value, std::initializer_list<const char*> options) {
    const auto key = value.toString();
    for (const auto* option : options) {
        if (key == QLatin1StringView(option)) {
            return key;
        }
    }
    return QString::fromLatin1(*options.begin());
}

}  // namespace

QJsonObject video_settings(const QJsonObject& stored) {
    QJsonObject result;
    for (const auto& entry : numbers) {
        const auto key = QLatin1StringView(entry.key);
        const auto value = stored.value(key).toDouble(entry.initial);
        result.insert(key, std::isfinite(value)
            ? std::clamp(value, entry.minimum, entry.maximum) : entry.initial);
    }
    for (const auto* name : {"videoAutoPlayNext", "videoRememberPlaybackPosition",
                            "videoAutoPauseOnLookup", "videoHardwareDecodingEnabled",
                            "videoSubtitleBackgroundDisabled"}) {
        const auto key = QLatin1StringView(name);
        result.insert(key, stored.value(key).toBool(true));
    }
    for (const auto* name : {"videoSubtitleGapFastForwardEnabled", "videoDeinterlacingEnabled",
                            "videoHDREnhancementEnabled", "videoRespectASSStyle",
                            "videoSubtitleMaskEnabled"}) {
        const auto key = QLatin1StringView(name);
        result.insert(key, stored.value(key).toBool(false));
    }
    for (const auto* name : {"videoMiningHistoryLimit", "videoSubtitleFontWeight", "studySidebarWidth"}) {
        const auto key = QLatin1StringView(name);
        result.insert(key, static_cast<int>(result.value(key).toDouble()));
    }
    result.insert("desktopLookupHoverDelayMs", stored.value("desktopLookupHoverDelayMs").toInt(45));
    result.insert("videoSubtitleFontFamily", stored.value("videoSubtitleFontFamily").toString());
    result.insert("videoShaderPreset", choice(stored.value("videoShaderPreset"),
        {"off", "anime4KFast", "anime4KHighQuality"}));
    result.insert("videoControlBarLayout", choice(stored.value("videoControlBarLayout"),
        {"floating", "compactBottom"}));
    result.insert("videoSubtitleMaskMode", choice(stored.value("videoSubtitleMaskMode"),
        {"Blur", "Transparent"}));
    const auto edge = stored.value("videoSubtitleEdgeStyle");
    const auto legacy_shadow = stored.value("videoSubtitleShadowRadius");
    if (!edge.isString() && legacy_shadow.isDouble() && std::isfinite(legacy_shadow.toDouble())) {
        result.insert("videoSubtitleEdgeStyle", QStringLiteral("softShadow"));
        result.insert("videoSubtitleEdgeStrength", std::clamp(legacy_shadow.toDouble() / 10, 0.0, 1.0));
    } else {
        result.insert("videoSubtitleEdgeStyle", choice(edge,
            {"highContrast", "off", "softShadow", "clearOutline"}));
        if (!edge.isString()) {
            result.insert("videoSubtitleEdgeStrength", 0.5);
        }
    }
    const auto legacy_position = stored.value("videoSubtitleVerticalPosition");
    if (!stored.value("videoSubtitleVerticalPositionFraction").isDouble()
        && legacy_position.isDouble() && std::isfinite(legacy_position.toDouble())) {
        const auto position = legacy_position.toDouble();
        const double fraction = position >= 0 ? 0.9 * (1 - std::min(position / 200, 1.0))
            : 0.9 + (1 - 0.9) * std::min(-position / 200, 1.0);
        result.insert("videoSubtitleVerticalPositionFraction", fraction);
    }
    const auto foreground = color_hex(stored.value("videoSubtitleColor").toString(),
                                      QStringLiteral("#FFFFFF"));
    result.insert("videoSubtitleColor", foreground);
    result.insert("videoSubtitleLookupHighlightColor",
        color_hex(stored.value("videoSubtitleLookupHighlightColor").toString(),
                  QStringLiteral("#B5C1CB3E")));
    result.insert("videoSubtitleLookupHighlightTextColor",
        color_hex(stored.value("videoSubtitleLookupHighlightTextColor").toString(), foreground));
    return result;
}

QJsonObject load_video_settings(const std::filesystem::path& directory) {
    return video_settings(directory.empty() ? QJsonObject{}
        : storage::read_json_file(directory / "settings.json").document.object());
}

bool store_video_settings(const std::filesystem::path& directory, const QJsonObject& changes) {
    if (directory.empty() || changes.isEmpty()) {
        return false;
    }
    const auto path = directory / "settings.json";
    const auto read = storage::read_json_file(path);
    if (read.status == storage::JsonReadStatus::invalid ||
        (read.status == storage::JsonReadStatus::success && !read.document.isObject())) {
        return false;
    }
    auto stored = read.document.object();
    const auto previous = video_settings(stored);
    for (auto item = changes.begin(); item != changes.end(); ++item) {
        if (!previous.contains(item.key()) || item.value().type() != previous.value(item.key()).type()) {
            return false;
        }
        stored.insert(item.key(), item.value());
    }
    if (changes.contains("videoSubtitleEdgeStrength") && !stored.value("videoSubtitleEdgeStyle").isString()) {
        stored.insert("videoSubtitleEdgeStyle", previous.value("videoSubtitleEdgeStyle"));
    }
    const auto resolved = video_settings(stored);
    for (auto item = changes.begin(); item != changes.end(); ++item) {
        stored.insert(item.key(), resolved.value(item.key()));
    }
    return storage::write_json_file(path, QJsonDocument(stored));
}

QJsonObject subtitle_appearance_defaults() {
    const auto defaults = video_settings();
    QJsonObject result;
    for (const auto* key : {"videoRespectASSStyle", "videoSubtitleFontFamily", "videoSubtitleFontSize",
                            "videoSubtitleFontWeight", "videoSubtitleEdgeStyle", "videoSubtitleEdgeStrength",
                            "videoSubtitleBackgroundOpacity", "videoSubtitleBackgroundDisabled",
                            "videoSubtitleVerticalPositionFraction", "videoSubtitleColor",
                            "videoSubtitleLookupHighlightColor", "videoSubtitleLookupHighlightTextColor"}) {
        result.insert(QLatin1StringView(key), defaults.value(QLatin1StringView(key)));
    }
    return result;
}

QString mpv_color(const QString& rgba) {
    const auto color = color_hex(rgba, QStringLiteral("#FFFFFF"));
    return color.size() == 9 ? u'#' + color.right(2) + color.mid(1, 6) : color;
}

SubtitleEdges subtitle_edges(const QString& style, double strength, double size) {
    strength = std::isfinite(strength) ? std::clamp(strength, 0.0, 1.0) : 0.5;
    const double scale = std::clamp(size / 36, 0.5, 2.0);
    const double outline = std::min(4.0, 2.5 * strength * scale);
    if (style == QStringLiteral("softShadow")) {
        return {0, std::min(8.0, 6 * strength * scale)};
    }
    if (style == QStringLiteral("clearOutline")) {
        return {outline, 0};
    }
    if (style == QStringLiteral("highContrast")) {
        return {std::min(2.5, 1.5 * outline), 0};
    }
    return {};
}

}  // namespace niratan::player
