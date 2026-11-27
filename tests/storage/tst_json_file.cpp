// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/storage/json_file.h"

#include <QDir>
#include <QFile>
#include <QJsonParseError>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QJsonObject>
#include <QTest>

#include <filesystem>
#include <string>

using niratan::storage::JsonReadResult;
using niratan::storage::JsonReadStatus;
using niratan::storage::read_json_file;
using niratan::storage::write_json_file;

// SPEC.md 3.5 and NiratanJsonFileStore.cs define the read statuses, the
// atomic write and the preserved damaged file; the cases follow those rules.
class JsonFileTest : public QObject {
    Q_OBJECT

   private slots:
    void a_missing_file_is_reported_as_missing();
    void a_valid_object_round_trips();
    void damaged_content_is_invalid_with_a_reason();
    void an_empty_file_is_invalid();
    void writing_creates_the_parent_directories();
    void writing_replaces_a_valid_file();
    void writing_refuses_to_replace_a_damaged_file();
    void a_failed_write_leaves_no_temporary_file();
};

namespace {

std::filesystem::path path_of(const QTemporaryDir& directory, const QString& name) {
    return std::filesystem::path(directory.filePath(name).toStdString());
}

QJsonDocument object_document(const char* key, const char* value) {
    return QJsonDocument(QJsonObject{{QString::fromUtf8(key), QString::fromUtf8(value)}});
}

bool has_entry_starting_with(const QTemporaryDir& directory, const QString& prefix) {
    const QStringList entries =
        QDir(directory.path()).entryList(QDir::Files | QDir::Hidden | QDir::System);
    for (const QString& entry : entries) {
        if (entry.startsWith(prefix)) {
            return true;
        }
    }
    return false;
}

}  // namespace

void JsonFileTest::a_missing_file_is_reported_as_missing() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const JsonReadResult result = read_json_file(path_of(directory, QStringLiteral("absent.json")));

    QCOMPARE(result.status, JsonReadStatus::missing);
    QVERIFY(result.document.isNull());
    QVERIFY(result.error.empty());
}

void JsonFileTest::a_valid_object_round_trips() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path path = path_of(directory, QStringLiteral("metadata.json"));

    QVERIFY(write_json_file(path, object_document("title", "化物語")));
    const JsonReadResult result = read_json_file(path);

    QCOMPARE(result.status, JsonReadStatus::success);
    QVERIFY(result.error.empty());
    QCOMPARE(result.document.object().value("title").toString(), QString::fromUtf8("化物語"));
}

void JsonFileTest::damaged_content_is_invalid_with_a_reason() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path path = path_of(directory, QStringLiteral("bookmark.json"));
    QFile file(QString::fromStdString(path.string()));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{\"progress\":");
    file.close();

    const JsonReadResult result = read_json_file(path);

    QCOMPARE(result.status, JsonReadStatus::invalid);
    QVERIFY(result.document.isNull());
    QVERIFY(!result.error.empty());
}

void JsonFileTest::an_empty_file_is_invalid() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path path = path_of(directory, QStringLiteral("statistics.json"));
    QFile file(QString::fromStdString(path.string()));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    QCOMPARE(read_json_file(path).status, JsonReadStatus::invalid);
}

void JsonFileTest::writing_creates_the_parent_directories() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path path =
        std::filesystem::path(directory.path().toStdString()) / "book" / "sidecar" /
        "metadata.json";

    QVERIFY(write_json_file(path, object_document("title", "Book")));
    QCOMPARE(read_json_file(path).status, JsonReadStatus::success);
}

void JsonFileTest::writing_replaces_a_valid_file() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path path = path_of(directory, QStringLiteral("metadata.json"));
    QVERIFY(write_json_file(path, object_document("title", "Old")));

    QVERIFY(write_json_file(path, object_document("title", "New")));

    QCOMPARE(read_json_file(path).document.object().value("title").toString(), QString("New"));
}

void JsonFileTest::writing_refuses_to_replace_a_damaged_file() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path path = path_of(directory, QStringLiteral("bookinfo.json"));
    QFile file(QString::fromStdString(path.string()));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not json");
    file.close();

    std::string error;
    QVERIFY(!write_json_file(path, object_document("title", "New"), &error));
    QVERIFY(!error.empty());

    QFile untouched(QString::fromStdString(path.string()));
    QVERIFY(untouched.open(QIODevice::ReadOnly));
    QCOMPARE(untouched.readAll(), QByteArray("not json"));
}

void JsonFileTest::a_failed_write_leaves_no_temporary_file() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path path = path_of(directory, QStringLiteral("metadata.json"));
    QFile file(QString::fromStdString(path.string()));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not json");
    file.close();

    std::string error;
    QVERIFY(!write_json_file(path, object_document("title", "New"), &error));

    QVERIFY(!has_entry_starting_with(directory, QStringLiteral("metadata.json.")));
}

QTEST_APPLESS_MAIN(JsonFileTest)
#include "tst_json_file.moc"
