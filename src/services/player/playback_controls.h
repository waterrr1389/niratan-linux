// SPDX-License-Identifier: GPL-3.0-or-later
//
// PlaybackEngine.swift (VideoPlaybackSpeed) and VideoControlsView.swift
// (commitSpeedInput): 0.25 is a distinct preset, custom values round to
// tenths, non-finite numeric input means normal speed, and unparseable text
// leaves speed unchanged.
//
// VideoPlayerViewModel.swift (setABLoopStart, setABLoopEnd, clearABLoop):
// endpoints clamp to the media bounds, A remains pending until a valid B,
// and replacing B keeps the existing A. Non-finite endpoints are rejected.

#pragma once

#include <QString>
#include <QStringView>

#include <optional>

namespace niratan::player {

double normalized_speed(double speed);
QString speed_label(double speed);
std::optional<double> parse_speed(QStringView text);

class ABLoop {
public:
    std::optional<double> start() const;
    std::optional<double> end() const;
    bool set_start(double time, double duration);
    bool set_end(double time, double duration);
    void clear();

private:
    std::optional<double> start_;
    std::optional<double> end_;
};

}  // namespace niratan::player
