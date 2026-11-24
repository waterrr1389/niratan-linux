// SPDX-License-Identifier: GPL-3.0-or-later

#include "oauth_policy.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace niratan::sync {

bool permits_stored_scope(std::optional<std::string_view> scope) {
    return !scope || scope_includes(scope, file_scope);
}

bool scope_includes(std::optional<std::string_view> scope, std::string_view required) {
    if (!scope) {
        return false;
    }
    const auto value = QString::fromUtf8(scope->data(), static_cast<qsizetype>(scope->size()));
    const auto scope_name =
        QString::fromUtf8(required.data(), static_cast<qsizetype>(required.size()));
    static const QRegularExpression whitespace(QStringLiteral("\\s+"),
                                               QRegularExpression::UseUnicodePropertiesOption);
    const auto grants = value.split(whitespace, Qt::SkipEmptyParts);
    return grants.contains(scope_name) ||
        (required == file_scope &&
         grants.contains(QStringLiteral("https://www.googleapis.com/auth/drive")));
}

std::optional<std::string> granted_scope(std::optional<std::string_view> response,
                                         std::string_view requested) {
    const auto granted = response.value_or(requested);
    return scope_includes(granted, requested) ? std::optional{std::string(granted)} : std::nullopt;
}

bool invalidates_stored_credentials(std::optional<int> status_code, QByteArrayView body) {
    return status_code == 400 &&
        QJsonDocument::fromJson(body.toByteArray()).object().value("error").toString() ==
        QStringLiteral("invalid_grant");
}

bool valid_client_id(std::string_view client_id) {
    static const QRegularExpression expression(
        QStringLiteral("\\A[0-9]+-[a-z0-9]+\\.apps\\.googleusercontent\\.com\\z"));
    return expression
        .match(QString::fromUtf8(client_id.data(), static_cast<qsizetype>(client_id.size())))
        .hasMatch();
}

}  // namespace niratan::sync
