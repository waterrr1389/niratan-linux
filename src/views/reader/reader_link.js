// SPDX-License-Identifier: GPL-3.0-or-later
//
// Same-spine link adaptation for ReaderPage, following NativeReaderView's
// jumpToLink same-spine branch and WebKit coordinator's internal jump
// handling. A QString fragment reaches QML as "" whether the activated URL
// omitted it or ended with '#', so presence is read from the URL itself. The
// browser runs the jump, waits for the async reader method, and reports the
// settled progress with the initiating token; only that revision is accepted.

.pragma library

function fragmentPresent(url) {
    return url.toString().indexOf("#") >= 0
}

function jumpScript(present, fragment) {
    const call = present
        ? "await window.hoshiReader?.jumpToFragment?.(" + JSON.stringify(fragment) + ");"
        : "await window.hoshiReader?.restoreProgress?.(0);"
    return "(async function() { "
        + "const token = window.hoshiReaderRestoreToken; "
        + "window.hoshiInternalJumpCount = (window.hoshiInternalJumpCount || 0) + 1; "
        + "try { " + call + " } catch (error) {} "
        + "await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))); "
        + "const progress = window.hoshiReader?.calculateProgress?.(); "
        + "window.hoshiInternalJumpCount -= 1; "
        + "if (window.hoshiInternalJumpCount === 0) { "
        + "window.hoshiSend?.('internalJumpCompleted', JSON.stringify({ "
        + "token: token, progress: Number.isFinite(progress) ? progress : null })); } })()"
}
