// SPDX-License-Identifier: GPL-3.0-or-later
//
// Features/Popup/PopupWebView.swift (Coordinator.userContentController):
// request/reply handlers settle the page's promises; feedback and selection
// are one-way. The inline lookup handlers (getEntries, lookupRedirect,
// queryTextSelected) work the same way the mining handlers do.

(() => {
    if (!window.renderPopup || !window.qt?.webChannelTransport) return;

    new QWebChannel(qt.webChannelTransport, channel => {
        const bridge = channel.objects.dictionaryBridge;
        if (!bridge) return;
        window.webkit ??= {};
        const messageHandlers = window.webkit.messageHandlers ??= {};
        const pending = new Map();
        let nextRequest = window.hoshiMiningRequestBase ?? 0;
        bridge.replyReady?.connect((id, payload) => {
            const resolve = pending.get(id);
            if (resolve) {
                pending.delete(id);
                resolve(payload);
            }
        });

        const request = (method, data, fallback) => {
            if (!bridge[method] || !bridge.replyReady) return Promise.resolve(fallback);
            return new Promise(resolve => {
                const id = ++nextRequest;
                pending.set(id, resolve);
                bridge[method](id, data);
            });
        };
        if (bridge.playWordAudio) {
            messageHandlers.playWordAudio = { postMessage: data => bridge.playWordAudio(data.url) };
        }
        if (bridge.showAudioSourceMenu) {
            messageHandlers.audioSourceMenu = { postMessage: data => bridge.showAudioSourceMenu(data) };
        }
        messageHandlers.mineEntry = {
            postMessage: data => request('mineEntry', data, {
                status: 'failed', message: 'Unable to add card.'
            })
        };
        messageHandlers.duplicateCheck = {
            postMessage: word => request('duplicateCheck', word)
        };
        messageHandlers.openAnkiNote = {
            postMessage: async ids => {
                if (Array.isArray(ids) && ids.every(id => typeof id === 'string')) {
                    document.activeElement?.blur();
                    await new Promise(resolve => setTimeout(resolve, 0));
                    bridge.openAnkiNote?.(ids);
                }
            }
        };
        messageHandlers.miningFeedback = { postMessage: data => bridge.miningFeedback?.(data) };
        messageHandlers.getEntries = {
            postMessage: data => request('getEntries', data, [])
        };
        messageHandlers.lookupRedirect = {
            postMessage: query => request('lookupRedirect', query, 0)
        };
        messageHandlers.queryTextSelected = {
            postMessage: data => request('queryTextSelected', data,
                { entryCount: 0, matchedCharacterCount: 0 })
        };
        messageHandlers.textSelected = { postMessage: data => bridge.textSelected?.(data) };
        if (window.hoshiMiningOptions) Object.assign(window, window.hoshiMiningOptions);
        window.hoshiUseInlineActionButtons = true;
        window.renderPopup();
    });
})();
