// SPDX-License-Identifier: GPL-3.0-or-later

#include "viewmodels/anki/anki_settings_view_model.h"

#include <algorithm>
#include <utility>

#include "services/anki/anki_config.h"
#include "services/anki/anki_requests.h"
#include "services/anki/fetch_metadata.h"
#include "services/anki/mapping_options.h"
#include "services/dictionary/term_dictionaries.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_roots.h"

namespace niratan::viewmodels {
namespace {

QStringList string_list(const std::vector<std::string>& strings) {
    QStringList list;
    for (const auto& string : strings) {
        list.append(QString::fromStdString(string));
    }
    return list;
}

QString text(const std::optional<std::string>& value) {
    return value ? QString::fromStdString(*value) : QString{};
}

struct PendingMetadata {
    std::vector<std::string> decks;
    std::vector<std::string> model_names;
    std::vector<anki::NoteType> note_types;
    std::size_t next_model = 0;
};

}  // namespace

struct AnkiSettingsViewModel::State {
    explicit State(AnkiSettingsViewModel& view_model) : owner(view_model) {}

    const anki::AnkiSettings& settings() const {
        static const anki::AnkiSettings defaults;
        return config ? config->settings() : defaults;
    }

    const anki::NoteType* selected_model() const {
        const auto& current = settings();
        if (!current.selected_note_type) {
            return nullptr;
        }
        const auto& models = current.available_note_types;
        const auto found =
            std::ranges::find(models, *current.selected_note_type, &anki::NoteType::name);
        return found == models.end() ? nullptr : &*found;
    }

    // AnkiManager.autofillFieldMappings, shared with profile activation
    // through the store; the callers save only when it returns true.
    bool autofill() { return config.has_value() && config->autofill_field_mappings(); }

    void set_error(QString message) {
        if (error != message) {
            error = std::move(message);
            emit owner.errorMessageChanged();
        }
    }

    void set_connected(bool value) {
        if (connected != value) {
            connected = value;
            emit owner.connectedChanged();
        }
    }

    void set_busy(bool value) {
        if (busy != value) {
            busy = value;
            emit owner.busyChanged();
        }
    }

    void record_save(const anki::AnkiSaveResult& result) {
        if (!result.ok) {
            set_error(QString::fromStdString(result.error));
        }
    }

    void save_changed() {
        record_save(config->save());
        emit owner.settingsChanged();
    }

    template <typename Edit> void edit(Edit update) {
        if (config) {
            update(config->settings());
            save_changed();
        }
    }

    void repair_address(bool save_before_ping) {
        auto& url = config->settings().transport->url;
        const bool repaired = text(url).trimmed().isEmpty();
        if (repaired) {
            url = anki::AnkiTransportConfig{}.url;
        }
        if (save_before_ping) {
            record_save(config->save());
        }
        if (repaired) {
            emit owner.settingsChanged();
        }
    }

    void discard_client() {
        if (auto* previous = std::exchange(client, nullptr)) {
            previous->deleteLater();
        }
    }

    void reset_connection() {
        discard_client();
        set_connected(false);
        set_busy(false);
    }

    void request(anki::Request request, anki::AnkiConnectClient::Callback callback) {
        const auto& transport = *settings().transport;
        const auto url = transport.url.value_or(std::string{});
        if (!client || connection.url != url || connection.api_key != transport.api_key) {
            discard_client();
            connection.url = url;
            connection.api_key = transport.api_key;
            client = new anki::AnkiConnectClient(connection, &owner);
        }
        client->request(std::move(request.action), std::move(request.params),
                        [this, source = client,
                         callback = std::move(callback)](anki::AnkiConnectResult result) {
                            if (source == client) {
                                callback(std::move(result));
                            }
                        });
    }

    void ping(bool fetch, bool save_before_ping) {
        if (!config || busy) {
            return;
        }
        repair_address(save_before_ping);
        set_busy(true);
        request(anki::version_request(), [this, fetch](anki::AnkiConnectResult result) {
            set_connected(anki::request_succeeded(result));
            if (connected) {
                record_save(config->save());
                if (fetch) {
                    fetch_decks();
                    return;
                }
            } else if (fetch) {
                set_error(
                    owner.tr("AnkiConnect is not connected. Please start Anki and try again."));
            }
            set_busy(false);
        });
    }

    bool check_fetch_reply(const anki::AnkiConnectResult& reply) {
        if (!reply.ok) {
            set_connected(false);
            set_error(QString::fromStdString(reply.error));
            set_busy(false);
            return false;
        }
        return true;
    }

    void fetch_decks() {
        request(anki::deck_names_request(), [this](anki::AnkiConnectResult reply) {
            if (!check_fetch_reply(reply)) {
                return;
            }
            auto decks = anki::parse_string_array_result(reply);
            if (!decks) {
                set_busy(false);
                return;
            }
            fetch_models(std::move(*decks));
        });
    }

    void fetch_models(std::vector<std::string> decks) {
        request(anki::model_names_request(),
                [this, decks = std::move(decks)](anki::AnkiConnectResult reply) mutable {
                    if (!check_fetch_reply(reply)) {
                        return;
                    }
                    auto models = anki::parse_string_array_result(reply);
                    if (!models) {
                        set_busy(false);
                        return;
                    }
                    fetch_fields({std::move(decks), std::move(*models), {}});
                });
    }

    void fetch_fields(PendingMetadata pending) {
        if (pending.next_model == pending.model_names.size()) {
            apply_metadata(std::move(pending));
            return;
        }
        auto field_request =
            anki::model_field_names_request(pending.model_names[pending.next_model]);
        request(
            std::move(field_request),
            [this, pending = std::move(pending)](anki::AnkiConnectResult reply) mutable {
                if (!check_fetch_reply(reply)) {
                    return;
                }
                if (auto fields = anki::parse_string_array_result(reply)) {
                    pending.note_types.push_back(
                        {std::move(pending.model_names[pending.next_model]), std::move(*fields)});
                }
                ++pending.next_model;
                fetch_fields(std::move(pending));
            });
    }

    void apply_metadata(PendingMetadata pending) {
        auto& current = config->settings();
        auto result = anki::apply_fetched_metadata(
            std::move(pending.decks), std::move(pending.note_types), current.selected_deck,
            current.selected_note_type, current.field_mappings);
        if (result) {
            current.available_decks = std::move(result->decks);
            current.available_note_types = std::move(result->note_types);
            current.selected_deck = std::move(result->selected_deck);
            current.selected_note_type = std::move(result->selected_note_type);
            current.field_mappings = std::move(result->mappings);
            save_changed();
        } else {
            set_error(owner.tr("No decks or models were returned from Anki."));
            record_save(config->save());
        }
        set_busy(false);
    }

    AnkiSettingsViewModel& owner;
    std::optional<profiles::ProfileRepository> repository;
    std::optional<anki::AnkiConfigStore> config;
    QStringList options;
    anki::AnkiConnectConfig connection;
    anki::AnkiConnectClient* client = nullptr;
    bool connected = false;
    bool busy = false;
    QString error;
};

AnkiSettingsViewModel::AnkiSettingsViewModel(QObject* parent)
    : QObject(parent), state_(std::make_unique<State>(*this)) {}

AnkiSettingsViewModel::~AnkiSettingsViewModel() = default;

QString AnkiSettingsViewModel::address() const {
    // The store repairs missing transport and URL on load.
    return text(state_->settings().transport->url);
}

QString AnkiSettingsViewModel::api_key() const {
    return text(state_->settings().transport->api_key);
}

QString AnkiSettingsViewModel::selected_deck() const {
    return text(state_->settings().selected_deck);
}

QString AnkiSettingsViewModel::selected_note_type() const {
    return text(state_->settings().selected_note_type);
}

QStringList AnkiSettingsViewModel::available_decks() const {
    return string_list(state_->settings().available_decks);
}

QStringList AnkiSettingsViewModel::available_note_types() const {
    QStringList list;
    for (const auto& type : state_->settings().available_note_types) {
        list.append(QString::fromStdString(type.name));
    }
    return list;
}

bool AnkiSettingsViewModel::has_selected_note_type() const {
    return state_->selected_model() != nullptr;
}

QStringList AnkiSettingsViewModel::fields() const {
    const auto* model = state_->selected_model();
    return model ? string_list(model->fields) : QStringList{};
}

QVariantMap AnkiSettingsViewModel::field_mappings() const {
    QVariantMap values;
    for (const auto& [field, value] : state_->settings().field_mappings) {
        values.insert(QString::fromStdString(field), QString::fromStdString(value));
    }
    return values;
}

QStringList AnkiSettingsViewModel::mapping_options() const {
    return state_->options;
}

bool AnkiSettingsViewModel::has_field_defaults() const {
    const auto* model = state_->selected_model();
    return model && anki::has_field_defaults(model->name);
}

QString AnkiSettingsViewModel::tags() const {
    return QString::fromStdString(state_->settings().tags);
}

bool AnkiSettingsViewModel::allow_duplicates() const {
    return state_->settings().allow_dupes;
}

QString AnkiSettingsViewModel::duplicate_scope() const {
    const auto& transport = state_->settings().transport;
    if (transport && transport->duplicate_scope == anki::DuplicateScope::deck) {
        return QStringLiteral("deck");
    }
    if (transport && transport->duplicate_scope == anki::DuplicateScope::deck_root) {
        return QStringLiteral("deckroot");
    }
    return QStringLiteral("collection");
}

bool AnkiSettingsViewModel::check_all_models() const {
    const auto& transport = state_->settings().transport;
    return transport && transport->check_all_models;
}

bool AnkiSettingsViewModel::compress_images() const {
    return state_->settings().compress_images;
}

bool AnkiSettingsViewModel::compact_glossaries() const {
    return state_->settings().compact_glossaries;
}

bool AnkiSettingsViewModel::connected() const {
    return state_->connected;
}

bool AnkiSettingsViewModel::busy() const {
    return state_->busy;
}

QString AnkiSettingsViewModel::error_message() const {
    return state_->error;
}

void AnkiSettingsViewModel::open() {
    state_->reset_connection();
    state_->config.reset();
    state_->repository = profiles::ProfileRepository::at_data_root();
    std::vector<anki::TermDictionary> dictionaries;
    if (state_->repository) {
        state_->config.emplace(storage::data_root(), *state_->repository);
        state_->config->load();
        if (state_->autofill()) {
            state_->record_save(state_->config->save());
        }
        dictionaries = dictionary::active_term_dictionaries(*state_->repository);
    }
    state_->options = string_list(anki::mapping_options(dictionaries));
    emit settingsChanged();
    state_->ping(false, false);
}

void AnkiSettingsViewModel::close() {
    if (state_->config) {
        state_->record_save(state_->config->save());
    }
    state_->config.reset();
    state_->reset_connection();
    emit settingsChanged();
}

void AnkiSettingsViewModel::connectToAnki() {
    state_->ping(false, true);
}

void AnkiSettingsViewModel::fetchMetadata() {
    state_->ping(true, false);
}

void AnkiSettingsViewModel::applyDefaults() {
    if (const auto* model = state_->selected_model();
        model && anki::has_field_defaults(model->name)) {
        auto& mappings = state_->config->settings().field_mappings;
        auto updated = anki::apply_default_mappings(model->name, model->fields, mappings);
        if (mappings != updated) {
            mappings = std::move(updated);
            state_->save_changed();
        }
    }
}

void AnkiSettingsViewModel::dismissError() {
    state_->set_error({});
}

void AnkiSettingsViewModel::setFieldMapping(const QString& field, const QString& mapping) {
    if (state_->config) {
        state_->record_save(
            state_->config->set_field_mapping(field.toStdString(), mapping.toStdString()));
        emit settingsChanged();
    }
}

void AnkiSettingsViewModel::setAddress(const QString& address) {
    if (state_->config) {
        state_->record_save(state_->config->set_anki_connect_url(address.toStdString()));
        emit settingsChanged();
    }
}

void AnkiSettingsViewModel::setApiKey(const QString& api_key) {
    if (state_->config) {
        state_->record_save(state_->config->set_anki_connect_api_key(api_key.toStdString()));
        emit settingsChanged();
    }
}

void AnkiSettingsViewModel::setSelectedDeck(const QString& deck) {
    state_->edit(
        [&](anki::AnkiSettings& settings) { settings.selected_deck = deck.toStdString(); });
}

void AnkiSettingsViewModel::setSelectedNoteType(const QString& note_type) {
    if (state_->config) {
        state_->config->settings().selected_note_type = note_type.toStdString();
        state_->autofill();
        state_->save_changed();
    }
}

void AnkiSettingsViewModel::setTags(const QString& tags) {
    if (state_->config) {
        state_->record_save(state_->config->set_tags(tags.toStdString()));
        emit settingsChanged();
    }
}

void AnkiSettingsViewModel::setAllowDuplicates(bool allow) {
    state_->edit([&](anki::AnkiSettings& settings) { settings.allow_dupes = allow; });
}

void AnkiSettingsViewModel::setDuplicateScope(const QString& scope) {
    auto value = anki::DuplicateScope::collection;
    if (scope == u"deck") {
        value = anki::DuplicateScope::deck;
    } else if (scope == u"deckroot") {
        value = anki::DuplicateScope::deck_root;
    } else if (scope != u"collection") {
        return;
    }
    state_->edit(
        [&](anki::AnkiSettings& settings) { settings.transport->duplicate_scope = value; });
}

void AnkiSettingsViewModel::setCheckAllModels(bool check) {
    state_->edit(
        [&](anki::AnkiSettings& settings) { settings.transport->check_all_models = check; });
}

void AnkiSettingsViewModel::setCompressImages(bool compress) {
    state_->edit([&](anki::AnkiSettings& settings) { settings.compress_images = compress; });
}

void AnkiSettingsViewModel::setCompactGlossaries(bool compact) {
    state_->edit([&](anki::AnkiSettings& settings) { settings.compact_glossaries = compact; });
}

}  // namespace niratan::viewmodels
