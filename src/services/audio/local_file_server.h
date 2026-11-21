// SPDX-License-Identifier: GPL-3.0-or-later
//
// The local media server, following Core/LocalFileServer.swift (respond,
// getCover, getSasayakiAudio, getAudioSources, getAudio, mimeType(forAudioFile:),
// parseRequest, send, setCover, setSasayakiAudio, setAudioServer, clearMedia,
// startServer, stopServer, scheduleStartRetry) and Core/LocalAudioEndpoint.swift.
//
// Routes, matched against the decoded request path in this order:
//   /cover/cover.        the cover bytes, application/octet-stream
//   /sasayaki/audio.m4a  the Sasayaki audio, audio/x-m4a
//   /localaudio/get/     one audio source list entry, application/json
//   /localaudio/<source>/<file>  the android table blob with the file's MIME type
//   anything else        404 text/plain
//
// A route with nothing to serve answers 404 with an empty body. A missing or
// unreadable local audio database answers the empty list for the source list
// and 404 for a blob. The source list is:
//   {"type":"audioSourceList","audioSources":[]}
// or one entry {"name": source, "url": "http://localhost:18765/localaudio/
// <source>/<percent-encoded file>"}. The empty body and the entry use the same
// JSON escaping rules as JSONSerialization: quote, backslash and control
// characters are escaped, everything else is UTF-8.
//
// The path and query are percent-decoded like Foundation's URLComponents.
// The file part is decoded a second time because the Swift handler calls
// removingPercentEncoding on it after the path was already decoded. Repeated
// query names keep the last value, as the Swift dictionary assignment does.
// The raw source is not decoded again and is inserted into the URL verbatim.
// A malformed percent sequence is decoded as far as QUrl can; Swift's
// removingPercentEncoding returns nil and keeps the original text instead.
//
// Every response is HTTP/1.1 with Content-Type, Content-Length and
// Connection: close. One request per connection.
//
// Lifecycle follows the Swift server: the listener runs while there is a
// cover, Sasayaki audio or enabled local audio, and stops when none remain.
// A failed listen is retried up to ten times, 300 ms apart, while there is
// something to serve. Unlike NWListener, the listener binds all interfaces but
// drops connections whose peer is not loopback, so the media stays on the
// machine; this is a Linux hardening decision and the only transport
// difference. The port is fixed at 18765 unless a caller passes another one
// for tests.
//
// The caller reads the cover file and passes the bytes; this class does not
// touch files other than the local audio database path.

#pragma once

#include <QByteArray>
#include <QObject>
#include <QTcpServer>
#include <QTimer>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace niratan::audio {

class LocalFileServer : public QObject {
    Q_OBJECT

   public:
    explicit LocalFileServer(std::string database_path, quint16 port = 18765,
                             QObject* parent = nullptr);
    ~LocalFileServer() override;

    // Each starts the listener when it has something to serve.
    void set_cover(QByteArray data);
    void set_sasayaki_audio(QByteArray data);

    // Drops both blobs and stops the listener when local audio is disabled.
    void clear_media();

    // Enabling restarts the listener; disabling stops it once nothing remains.
    void set_audio_enabled(bool enabled);

    // An active gallery route serves an image MIME type and bytes from the
    // caller's index. An empty callback removes the route.
    using BookImage = std::pair<std::string, QByteArray>;
    void set_book_images(std::string route,
                         std::function<std::optional<BookImage>(std::string_view)> lookup);

    bool is_listening() const;
    // The bound port, or 0 while not listening.
    quint16 port() const;

   private:
    void handle_connection();
    void handle_request(QTcpSocket* socket, const QByteArray& header);
    void start();
    void stop_if_idle();

    std::string database_path_;
    quint16 requested_port_;
    std::optional<QByteArray> cover_;
    std::optional<QByteArray> sasayaki_audio_;
    bool audio_enabled_ = false;
    std::string book_image_route_;
    std::function<std::optional<BookImage>(std::string_view)> book_image_lookup_;
    int retry_count_ = 0;
    QTcpServer server_;
    QTimer retry_timer_;
};

}  // namespace niratan::audio
