// SPDX-License-Identifier: GPL-3.0-or-later

#include "reader_shell.h"

#include <QFile>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <string>

namespace niratan::reader {
namespace {

// injectReader, horizontalSpreadColumnGap.
constexpr int kSpreadColumnGap = 32;

// Escapes the characters that would terminate or corrupt a single-quoted
// JavaScript string. injectReader uses a JSON literal, which escapes control
// characters too; reader tokens and expressions are plain text.
std::string js_string(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
            case '\\':
                escaped += "\\\\";
                break;
            case '\'':
                escaped += "\\'";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += ch;
                break;
        }
    }
    return escaped;
}

// Shortest round-trip form, so an integral size prints without a decimal part.
std::string number(double value) {
    std::array<char, 32> buffer{};
    const std::to_chars_result result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    return std::string(buffer.data(), result.ptr);
}

bool is_horizontal_spread(const Appearance& appearance) {
    return appearance.two_column && !appearance.vertical_writing && !appearance.continuous &&
           !appearance.paragraph_mode;
}

char ascii_lower(char ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return static_cast<char>(ch - 'A' + 'a');
    }
    return ch;
}

std::size_t find_ci(const std::string& text, const std::string& needle, std::size_t from = 0) {
    if (needle.empty() || needle.size() > text.size()) {
        return std::string::npos;
    }
    for (std::size_t i = from; i + needle.size() <= text.size(); ++i) {
        bool matches = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            if (ascii_lower(text[i + j]) != ascii_lower(needle[j])) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return i;
        }
    }
    return std::string::npos;
}

std::size_t find_last_ci(const std::string& text, const std::string& needle) {
    std::size_t found = std::string::npos;
    std::size_t from = 0;
    while (true) {
        const std::size_t position = find_ci(text, needle, from);
        if (position == std::string::npos) {
            return found;
        }
        found = position;
        from = position + 1;
    }
}

// window.webkit.messageHandlers as the upstream scripts know it from WKWebView.
// Each handler forwards the JSON text of its payload to the QWebChannel bridge.
// progressChanged is posted by the continuous progress block below.
constexpr const char* kBridgeShim = R"js(window.__hoshiPendingMessages = [];
window.__hoshiBridge = null;
window.hoshiSend = function(name, payload) {
    if (window.__hoshiBridge && typeof window.__hoshiBridge.message === 'function') {
        window.__hoshiBridge.message(name, payload);
        return;
    }
    window.__hoshiPendingMessages.push([name, payload]);
};
window.webkit = {
    messageHandlers: {
        textSelected: { postMessage: function(payload) {
            window.hoshiSend('textSelected', JSON.stringify({ ...payload, token: window.hoshiReaderRestoreToken }));
        } },
        selectionState: { postMessage: function(payload) { window.hoshiSend('selectionState', JSON.stringify(payload)); } },
        pageChanged: { postMessage: function(payload) { window.hoshiSend('pageChanged', JSON.stringify(payload)); } },
        pageStarts: { postMessage: function(payload) { window.hoshiSend('pageStarts', JSON.stringify(payload)); } },
        restoreCompleted: { postMessage: function(payload) {
            clearTimeout(window.hoshiRestoreTimeout);
            window.hoshiSend('restoreCompleted', JSON.stringify(payload));
        } },
        wheelNavigation: { postMessage: function(payload) { window.hoshiSend('wheelNavigation', JSON.stringify(payload)); } },
        tapOutside: { postMessage: function() {
            window.hoshiSend('tapOutside', JSON.stringify({ token: window.hoshiReaderRestoreToken }));
        } },
        imageTapped: { postMessage: function(payload) { window.hoshiSend('imageTapped', JSON.stringify(payload)); } },
        spineMeasured: { postMessage: function(payload) { window.hoshiSend('spineMeasured', JSON.stringify(payload)); } },
        progressChanged: { postMessage: function(payload) { window.hoshiSend('progressChanged', JSON.stringify(payload)); } }
    }
};
new QWebChannel(qt.webChannelTransport, function(channel) {
    window.__hoshiBridge = channel.objects.readerBridge;
    var pending = window.__hoshiPendingMessages;
    window.__hoshiPendingMessages = [];
    pending.forEach(function(entry) {
        window.hoshiSend(entry[0], entry[1]);
    });
});
)js";

// Posts reading progress while scrolling; NativeReaderView.injectReader emits it
// only in continuous mode.
constexpr const char* kScrollProgress = R"js(    if (!window.hoshiNativeProgressRegistered) {
        window.hoshiNativeProgressRegistered = true;
        var hoshiNativeProgressFrame = null;
        var hoshiNativeLastProgressPost = 0;
        const postNativeProgress = () => {
            hoshiNativeProgressFrame = null;
            const now = Date.now();
            if (now - hoshiNativeLastProgressPost < 250) { return; }
            hoshiNativeLastProgressPost = now;
            const progress = window.hoshiReader.calculateProgress?.();
            if (typeof progress === 'number' && Number.isFinite(progress)) {
                window.webkit?.messageHandlers?.progressChanged?.postMessage(progress);
            }
        };
        window.addEventListener('scroll', () => {
            if (hoshiNativeProgressFrame === null) {
                hoshiNativeProgressFrame = requestAnimationFrame(postNativeProgress);
            }
        }, { passive: true });
    }
)js";

}  // namespace

ReaderScripts load_reader_scripts() {
    const auto read = [](const QString& path) -> std::string {
        QFile file(path.startsWith(QStringLiteral(":/qtwebchannel/"))
                       ? path
                       : QStringLiteral(":/niratan/reader/") + path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return file.readAll().toStdString();
    };
    ReaderScripts scripts;
    scripts.selection = read(QStringLiteral("selection.js"));
    scripts.reader = read(QStringLiteral("reader.js"));
    scripts.scrollreader = read(QStringLiteral("scrollreader.js"));
    scripts.highlights = read(QStringLiteral("highlights.js"));
    scripts.paragraph = read(QStringLiteral("paragraph.js"));
    scripts.webchannel = read(QStringLiteral(":/qtwebchannel/qwebchannel.js"));
    return scripts;
}

std::string reader_document(const std::string& chapter_html, const ShellInput& input,
                            const ReaderScripts& scripts) {
    const std::string style = reader_style_block(input);
    const std::string script = reader_script_block(input, scripts);

    std::string document = chapter_html;

    // The stylesheet belongs in the head. A chapter without markers still gets
    // it, prepended so it applies to everything that follows.
    const std::size_t head_end = find_ci(document, "</head>");
    const std::size_t body_start = find_ci(document, "<body");
    const std::size_t style_at = head_end != std::string::npos ? head_end : body_start;
    if (style_at == std::string::npos) {
        document.insert(0, style);
    } else {
        document.insert(style_at, style);
    }

    // The script needs the body to exist, so it goes before the last closing
    // body tag; fragments without one get it appended.
    const std::size_t body_end = find_last_ci(document, "</body>");
    if (body_end == std::string::npos) {
        document += script;
    } else {
        document.insert(body_end, script);
    }

    return document;
}

std::string reader_style_block(const ShellInput& input) {
    const std::string css =
        reader_css(input.appearance, input.page, input.colors, input.font_face_css);
    return "<style>\n" + css + "\n</style>";
}

std::string reader_script_block(const ShellInput& input, const ReaderScripts& scripts) {
    const Appearance& appearance = input.appearance;
    const bool paragraph_mode = appearance.paragraph_mode && !appearance.continuous;
    const bool spread = is_horizontal_spread(appearance);
    const int columns = spread ? 2 : 1;
    const double spread_page_size =
        spread ? std::max(1.0, static_cast<double>(input.page.width) -
                                   static_cast<double>(input.page.width) *
                                       static_cast<double>(appearance.horizontal_padding) / 100.0 +
                                   static_cast<double>(kSpreadColumnGap))
               : static_cast<double>(input.page.width);

    std::string script;
    script += "<script>\n";
    script += "(function() {\n";

    script += "    window.hoshiReaderRestoreToken = '" + js_string(input.restore_token) + "';\n";

    // The upstream chapter carries its own viewport; replacing it disables
    // pinch zoom and keeps CSS pixels equal to device-independent pixels.
    script += R"js(    var viewport = document.querySelector('meta[name="viewport"]');
    if (viewport) { viewport.remove(); }
    var newViewport = document.createElement('meta');
    newViewport.name = 'viewport';
    newViewport.content = 'width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no';
    document.head.appendChild(newViewport);
)js";

    script += "    document.documentElement.style.setProperty('--page-height', '" +
              std::to_string(input.page.height) + "px');\n";
    script += "    document.documentElement.style.setProperty('--page-width', '" +
              std::to_string(input.page.width) + "px');\n";

    // The stylesheet is already in the document; only its colour variables are
    // resolved here, so a theme change does not need a new stylesheet.
    if (input.colors.text.empty()) {
        script += "    document.documentElement.style.removeProperty('--hoshi-text-color');\n";
    } else {
        script += "    document.documentElement.style.setProperty('--hoshi-text-color', '" +
                  input.colors.text + "');\n";
    }
    script += "    document.documentElement.style.setProperty('--hoshi-reader-background-color', ";
    script += "'" + input.colors.background + "');\n";
    script += "    document.documentElement.style.setProperty('--hoshi-sasayaki-text-color', ";
    script += "'" + input.colors.sasayaki_text + "');\n";
    script += "    document.documentElement.style.setProperty('--hoshi-sasayaki-background-color', ";
    script += "'" + input.colors.sasayaki_background + "');\n";

    script += "    window.scanNonJapaneseText = ";
    script += input.scan_non_japanese_text ? "true;\n" : "false;\n";

    script += reader_spacer_script(appearance);
    if (!script.empty() && script.back() != '\n') {
        script += '\n';
    }

    // qwebchannel.js comes first so QWebChannel exists, then the message shim,
    // then the channel connection. Every upstream script below can post
    // immediately because hoshiSend queues until the channel resolves.
    script += scripts.webchannel;
    if (!script.empty() && script.back() != '\n') {
        script += '\n';
    }
    script += kBridgeShim;

    script += scripts.selection;
    if (!script.empty() && script.back() != '\n') {
        script += '\n';
    }
    script += "    window.hoshiSelection.language = '" + input.content_language + "';\n";

    script += appearance.continuous ? scripts.scrollreader : scripts.reader;
    if (!script.empty() && script.back() != '\n') {
        script += '\n';
    }

    script += "    window.hoshiReader.measurePageStarts = ";
    script += input.measure_page_starts ? "true;\n" : "false;\n";

    script += "    window.hoshiReader.sharedSyncCoordinates = ";
    script += input.shared_sync_coordinates ? "true;\n" : "false;\n";

    script += scripts.highlights;
    if (!script.empty() && script.back() != '\n') {
        script += '\n';
    }

    if (paragraph_mode) {
        script += scripts.paragraph;
        if (!script.empty() && script.back() != '\n') {
            script += '\n';
        }
    }

    // NativeReaderView.injectReader: pictures are sized by the stylesheet, not
    // by the publisher. A large non-gaiji img becomes block-img, which the CSS
    // caps while keeping its ratio; Blur Images only adds the class and the
    // wrapper. Clicking a blurred illustration reveals it; any other
    // illustration reports its URL through imageTapped.
    script += "    function setupImage(element, src, wrap, blurElement = element) {\n";
    script += "        var target = element;\n";
    script += appearance.blur_images ? "        if (true) {\n" : "        if (false) {\n";
    script += R"js(            blurElement.classList.add('blurred');
            if (wrap) {
                target = document.createElement('div');
                target.className = 'blur-wrapper';
                blurElement.before(target);
                target.append(blurElement);
            }
        }
        target.onclick = event => {
            event.preventDefault();
            event.stopPropagation();
            if (blurElement.classList.contains('blurred')) {
                blurElement.classList.remove('blurred');
                return;
            }
            window.webkit.messageHandlers.imageTapped.postMessage(new URL(src, document.baseURI).href);
        };
    }
    document.querySelectorAll('svg[preserveAspectRatio="none"]').forEach(svg => svg.removeAttribute('preserveAspectRatio'));
    document.querySelectorAll('svg').forEach(svg => {
        var svgImage = svg.querySelector('image');
        if (!svgImage) {
            return;
        }
        setupImage(svgImage, svgImage.href.baseVal, false, svg);
    });
    Array.from(document.querySelectorAll('img')).forEach(img => {
        const processImg = () => {
            const isGaiji = img.classList.contains('gaiji') || img.classList.contains('gaiji-line');
            if (!isGaiji && (img.naturalWidth > 256 || img.naturalHeight > 256)) {
                img.classList.add('block-img');
                setupImage(img, img.src, true);
            }
        };
        if (img.complete) {
            if (img.naturalWidth > 0) {
                processImg();
            }
        } else {
            img.onload = processImg;
            img.onerror = () => {};
        }
    });
)js";

    script += "    const lookupScanLength = " + std::to_string(input.scan_length) + ";\n";
    script += R"js(    document.addEventListener('click', event => {
        if (event.button !== 0 ||
            event.target?.closest?.('a, button, input, textarea, select, [contenteditable="true"]')) {
            return;
        }
        if (window.hoshiParagraph?.finishTextAnimation()) {
            event.preventDefault();
            event.stopPropagation();
            return;
        }
        const selection = window.getSelection();
        if (selection && !selection.isCollapsed) { return; }
        if (!window.hoshiSelection.selectText(event.clientX, event.clientY, lookupScanLength)) {
            window.webkit.messageHandlers.tapOutside.postMessage();
        }
    }, true);
)js";
    script += "    window.hoshiSelection.registerModifierTracking();\n";
    script += "    window.hoshiSelection.registerShiftHoverLookup(" +
              std::to_string(input.scan_length) + ", " + std::to_string(input.hover_delay_ms) +
              ");\n";
    script += "    window.hoshiReader.pageHeight = " + std::to_string(input.page.height) + ";\n";
    script += "    window.hoshiReader.pageWidth = " + std::to_string(input.page.width) + ";\n";
    script += "    window.hoshiReader.horizontalPageColumns = " + std::to_string(columns) + ";\n";
    script += "    window.hoshiReader.horizontalSpreadPageSize = " + number(spread_page_size) +
              ";\n";
    script += "    window.hoshiReader.registerCopyText?.();\n";
    script += "    window.hoshiReader.registerWheelNavigation?.(";
    script += input.wheel_page_turn && !appearance.continuous ? "true" : "false";
    script += ");\n";

    if (appearance.continuous) {
        script += kScrollProgress;
    }

    if (!input.initial_restore_expression.empty()) {
        script += R"js(    let timedOut = false;
    const revealFallback = () => {
        timedOut = true;
        window.hoshiReader.notifyRestoreComplete();
    };
    window.hoshiRestoreTimeout = setTimeout(revealFallback, 2500);
    const readyImages = Array.from(document.images, image =>
        image.complete ? Promise.resolve() : image.decode().catch(() => {}));
    Promise.all([document.fonts.ready, ...readyImages]).then(() => {
        if (timedOut) { return; }
)js";
        if (paragraph_mode) {
            script += "        window.hoshiParagraph.layoutParagraphs();\n";
        }
        if (!input.highlights_json.empty()) {
            script += "        window.hoshiHighlights.applyHighlights(" + input.highlights_json +
                      ");\n";
        }
        script += "        return " + input.initial_restore_expression + ";\n";
        script += "    }).catch(revealFallback);\n";
    }

    script += "})();\n";
    script += "</script>";

    return script;
}

}  // namespace niratan::reader
