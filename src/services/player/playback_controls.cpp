// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/player/playback_controls.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace niratan::player {
namespace {

constexpr double kMinimumSpeed = 0.25;
constexpr double kCustomMinimum = 0.3;
constexpr double kMaximumSpeed = 5.0;
constexpr double kSpeedStep = 0.1;

double clamped_time(double time, double duration) {
    return duration > 0 ? std::clamp(time, 0.0, duration) : std::max(time, 0.0);
}

}  // namespace

double normalized_speed(double speed) {
    if (!std::isfinite(speed)) {
        return 1;
    }
    if (speed <= kMinimumSpeed) {
        return kMinimumSpeed;
    }
    return std::clamp(std::round(speed / kSpeedStep) * kSpeedStep, kCustomMinimum,
                      kMaximumSpeed);
}

QString speed_label(double speed) {
    const double value = normalized_speed(speed);
    const int precision = value == kMinimumSpeed ? 2 : (value == std::trunc(value) ? 0 : 1);
    return QString::number(value, 'f', precision);
}

std::optional<double> parse_speed(QStringView text) {
    const QString input = text.trimmed().toString().replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    const double value = input.toDouble(&ok);
    if (ok || std::isinf(value)) {
        return normalized_speed(value);
    }
    static const QRegularExpression non_finite(QStringLiteral("\\A[+-]?(?:inf(?:inity)?|nan)\\z"),
                                               QRegularExpression::CaseInsensitiveOption);
    if (non_finite.match(input).hasMatch()) {
        return 1.0;
    }
    return std::nullopt;
}

std::optional<double> ABLoop::start() const { return start_; }
std::optional<double> ABLoop::end() const { return end_; }

bool ABLoop::set_start(double time, double duration) {
    if (!std::isfinite(time)) {
        return false;
    }
    start_ = clamped_time(time, duration);
    end_.reset();
    return true;
}

bool ABLoop::set_end(double time, double duration) {
    if (!start_ || !std::isfinite(time)) {
        return false;
    }
    const double end = clamped_time(time, duration);
    if (end <= *start_) {
        return false;
    }
    end_ = end;
    return true;
}

void ABLoop::clear() {
    start_.reset();
    end_.reset();
}

}  // namespace niratan::player
