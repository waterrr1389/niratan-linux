// SPDX-License-Identifier: GPL-3.0-or-later
//
// Isolates a view model test from the developer's real data. View models
// build their services internally and read the data root through
// services/storage/app_paths, which consults the environment on every call,
// so pointing HOME and the XDG variables at a temporary directory before the
// first model or singleton is constructed keeps every read and write inside
// it.
//
// The previous values are restored when the guard goes out of scope.

#pragma once

#include <QByteArray>
#include <QTemporaryDir>

namespace niratan::test {

class TempDataRoot {
   public:
    TempDataRoot() {
        home_ = capture("HOME");
        data_home_ = capture("XDG_DATA_HOME");
        cache_home_ = capture("XDG_CACHE_HOME");
        const QByteArray base = dir_.path().toUtf8();
        qputenv("HOME", base);
        qputenv("XDG_DATA_HOME", base + "/data");
        qputenv("XDG_CACHE_HOME", base + "/cache");
    }

    TempDataRoot(const TempDataRoot&) = delete;
    TempDataRoot& operator=(const TempDataRoot&) = delete;

    ~TempDataRoot() { restore(); }

    bool isValid() const { return dir_.isValid(); }
    QString path() const { return dir_.path(); }

   private:
    struct Original {
        bool set = false;
        QByteArray value;
    };

    static Original capture(const char* name) {
        Original original;
        original.set = qEnvironmentVariableIsSet(name);
        original.value = qgetenv(name);
        return original;
    }

    static void restore_one(const char* name, const Original& original) {
        if (original.set) {
            qputenv(name, original.value);
        } else {
            qunsetenv(name);
        }
    }

    void restore() {
        restore_one("HOME", home_);
        restore_one("XDG_DATA_HOME", data_home_);
        restore_one("XDG_CACHE_HOME", cache_home_);
    }

    QTemporaryDir dir_;
    Original home_;
    Original data_home_;
    Original cache_home_;
};

}  // namespace niratan::test
