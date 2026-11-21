// SPDX-License-Identifier: GPL-3.0-or-later

(() => {
    if (!window.renderPopup) return;

    new QWebChannel(qt.webChannelTransport, channel => {
        const bridge = channel.objects.dictionaryBridge;
        window.webkit ??= {};
        window.webkit.messageHandlers ??= {};
        window.webkit.messageHandlers.playWordAudio = {
            postMessage: data => bridge.playWordAudio(data.url)
        };
        window.webkit.messageHandlers.audioSourceMenu = {
            postMessage: data => bridge.showAudioSourceMenu(data)
        };
        window.hoshiUseInlineActionButtons = true;
        window.renderPopup();
    });
})();
