// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/storage/app_paths.h"

#include <QTemporaryDir>
#include <QTest>

#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>

#include "models/dictionary/dictionary_config.h"

using niratan::dictionary::DictionaryType;
using niratan::storage::cache_root;
using niratan::storage::data_root;
using niratan::storage::dictionaries_root;
using niratan::storage::dictionary_type_directory;
using niratan::storage::resolve_within;

// SPEC.md 3.5 fixes the Linux roots and Helpers/AppDataHelper.cs fixes the
// internal names; the XDG base directory rules decide when a variable is
// usable. resolve_within follows the path confinement rule of SPEC.md 3.5.
namespace {

class EnvironmentGuard {
   public:
    explicit EnvironmentGuard(std::initializer_list<const char*> names) {
        for (const char* name : names) {
            Entry entry;
            entry.name = name;
            entry.value = qgetenv(name);
            entry.was_set = qEnvironmentVariableIsSet(name);
            entries_.push_back(entry);
        }
    }

    ~EnvironmentGuard() {
        for (const Entry& entry : entries_) {
            if (entry.was_set) {
                qputenv(entry.name.constData(), entry.value);
            } else {
                qunsetenv(entry.name.constData());
            }
        }
    }

    EnvironmentGuard(const EnvironmentGuard&) = delete;
    EnvironmentGuard& operator=(const EnvironmentGuard&) = delete;

   private:
    struct Entry {
        QByteArray name;
        QByteArray value;
        bool was_set = false;
    };

    std::vector<Entry> entries_;
};

std::string path_text(const std::filesystem::path& path) {
    return path.string();
}

}  // namespace

class AppPathsTest : public QObject {
    Q_OBJECT

   private slots:
    void xdg_variables_choose_the_roots();
    void home_is_the_fallback();
    void relative_xdg_values_are_ignored();
    void missing_home_yields_empty_roots();
    void the_dictionaries_root_is_below_the_data_root();
    void dictionary_types_have_their_own_directories();
    void resolve_within_keeps_relative_paths();
    void resolve_within_rejects_absolute_and_parent_paths();
};

void AppPathsTest::xdg_variables_choose_the_roots() {
    EnvironmentGuard guard({"XDG_DATA_HOME", "XDG_CACHE_HOME", "HOME"});
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    qputenv("XDG_DATA_HOME", directory.filePath(QStringLiteral("data")).toUtf8());
    qputenv("XDG_CACHE_HOME", directory.filePath(QStringLiteral("cache")).toUtf8());
    qputenv("HOME", directory.filePath(QStringLiteral("home")).toUtf8());

    const std::filesystem::path data(directory.filePath(QStringLiteral("data")).toStdString());
    const std::filesystem::path cache(directory.filePath(QStringLiteral("cache")).toStdString());
    QCOMPARE(path_text(data_root()), path_text(data / "niratan"));
    QCOMPARE(path_text(cache_root()), path_text(cache / "niratan"));
}

void AppPathsTest::home_is_the_fallback() {
    EnvironmentGuard guard({"XDG_DATA_HOME", "XDG_CACHE_HOME", "HOME"});
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    qunsetenv("XDG_DATA_HOME");
    qunsetenv("XDG_CACHE_HOME");
    qputenv("HOME", directory.filePath(QStringLiteral("home")).toUtf8());

    const std::filesystem::path home(directory.filePath(QStringLiteral("home")).toStdString());
    QCOMPARE(path_text(data_root()), path_text(home / ".local/share/niratan"));
    QCOMPARE(path_text(cache_root()), path_text(home / ".cache/niratan"));
}

void AppPathsTest::relative_xdg_values_are_ignored() {
    EnvironmentGuard guard({"XDG_DATA_HOME", "XDG_CACHE_HOME", "HOME"});
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    qputenv("XDG_DATA_HOME", "relative/data");
    qputenv("XDG_CACHE_HOME", "relative/cache");
    qputenv("HOME", directory.filePath(QStringLiteral("home")).toUtf8());

    const std::filesystem::path home(directory.filePath(QStringLiteral("home")).toStdString());
    QCOMPARE(path_text(data_root()), path_text(home / ".local/share/niratan"));
    QCOMPARE(path_text(cache_root()), path_text(home / ".cache/niratan"));
}

void AppPathsTest::missing_home_yields_empty_roots() {
    EnvironmentGuard guard({"XDG_DATA_HOME", "XDG_CACHE_HOME", "HOME"});
    qunsetenv("XDG_DATA_HOME");
    qunsetenv("XDG_CACHE_HOME");
    qunsetenv("HOME");

    QVERIFY(data_root().empty());
    QVERIFY(cache_root().empty());
}

void AppPathsTest::the_dictionaries_root_is_below_the_data_root() {
    EnvironmentGuard guard({"XDG_DATA_HOME", "XDG_CACHE_HOME", "HOME"});
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    qputenv("XDG_DATA_HOME", directory.filePath(QStringLiteral("data")).toUtf8());
    qunsetenv("XDG_CACHE_HOME");

    const std::filesystem::path data(directory.filePath(QStringLiteral("data")).toStdString());
    QCOMPARE(path_text(dictionaries_root()), path_text(data / "niratan/dictionaries"));
}

// B1: DictionaryType.rawValue names the typed directories.
void AppPathsTest::dictionary_types_have_their_own_directories() {
    EnvironmentGuard guard({"XDG_DATA_HOME", "XDG_CACHE_HOME", "HOME"});
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    qputenv("XDG_DATA_HOME", directory.filePath(QStringLiteral("data")).toUtf8());
    qunsetenv("XDG_CACHE_HOME");

    const std::filesystem::path data(directory.filePath(QStringLiteral("data")).toStdString());
    const auto expected = [&data](const char* name) {
        return path_text(data / "niratan/dictionaries" / name);
    };
    QCOMPARE(path_text(dictionary_type_directory(DictionaryType::term)), expected("Term"));
    QCOMPARE(path_text(dictionary_type_directory(DictionaryType::frequency)),
             expected("Frequency"));
    QCOMPARE(path_text(dictionary_type_directory(DictionaryType::pitch)), expected("Pitch"));
    QCOMPARE(path_text(dictionary_type_directory(DictionaryType::kanji)), expected("Kanji"));
}

void AppPathsTest::resolve_within_keeps_relative_paths() {
    const std::filesystem::path root = "/tmp/niratan/Novels/book";

    const auto nested = resolve_within(root, "media/cover.jpg");
    QVERIFY(nested.has_value());
    QCOMPARE(path_text(*nested), path_text(root / "media/cover.jpg"));

    const auto dotted = resolve_within(root, "a/./metadata.json");
    QVERIFY(dotted.has_value());
    QCOMPARE(path_text(*dotted), path_text(root / "a/metadata.json"));
}

void AppPathsTest::resolve_within_rejects_absolute_and_parent_paths() {
    const std::filesystem::path root = "/tmp/niratan/Novels/book";

    QVERIFY(!resolve_within(root, "/etc/passwd").has_value());
    QVERIFY(!resolve_within(root, "../outside.json").has_value());
    QVERIFY(!resolve_within(root, "a/../../outside.json").has_value());
    QVERIFY(!resolve_within(root, "a/..").has_value());
}

QTEST_APPLESS_MAIN(AppPathsTest)
#include "tst_app_paths.moc"
