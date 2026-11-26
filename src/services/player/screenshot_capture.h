// SPDX-License-Identifier: GPL-3.0-or-later
//
// The file half of VideoPlayerScreen.saveCleanScreenshot: mpv captures the
// picture into a temporary PNG, the save panel picks the destination, and the
// capture is then moved into place atomically. The capture lives under the
// platform temporary directory; discarding it removes the file.

#pragma once

#include <QString>

namespace niratan::player {

// A fresh path under the platform temporary directory, ending in .png.
QString temporary_screenshot_path();

// Copies sourcePath to destinationPath atomically and removes the captured
// temporary file. An empty destination only removes the capture, which is what
// the cancelled save panel does. False when the capture is missing or cannot
// be written; the temporary file is kept so a retry can still use it.
bool save_screenshot_capture(const QString& sourcePath, const QString& destinationPath);

}  // namespace niratan::player
