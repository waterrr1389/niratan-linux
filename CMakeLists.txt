cmake_minimum_required(VERSION 3.28)

project(niratan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The dictionary library is consumed as-is. Its own warnings must not fail
# this build, and its sources are never patched from here.
set(HOSHI_WARNINGS ${CMAKE_CXX_FLAGS})
add_subdirectory(third_party/hoshidicts EXCLUDE_FROM_ALL)
set(CMAKE_CXX_FLAGS ${HOSHI_WARNINGS})

find_package(Qt6 6.12 REQUIRED COMPONENTS Quick QuickControls2 Test Network WebEngineQuick
    WebChannelQuick QuickDialogs2 OpenGL Multimedia)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
qt_standard_project_setup(REQUIRES 6.12)
find_package(SQLite3 REQUIRED)
find_package(libzip REQUIRED)

qt_add_library(niratan_core STATIC
    src/models/dictionary/audio_source.cpp
    src/services/anki/field_mapping.cpp
    src/services/anki/duplicate_scope.cpp
    src/services/anki/duplicate_query.cpp
    src/services/anki/duplicate_lookup.cpp
    src/services/anki/glossary_markup.cpp
    src/services/anki/media_naming.cpp
    src/services/anki/field_content.cpp
    src/services/anki/anki_connect.cpp
    src/services/anki/anki_config.cpp
    src/services/anki/anki_config_codec.cpp
    src/services/anki/anki_requests.cpp
    src/services/anki/mining_gate.cpp
    src/services/audio/audio_request.cpp
    src/services/audio/local_audio.cpp
    src/services/audio/local_file_server.cpp
    src/services/dictionary/dictionary_store.cpp
    src/services/dictionary/dictionary_media.cpp
    src/services/dictionary/media_type.cpp
    src/services/dictionary/dictionary_query.cpp
    src/services/dictionary/frequency_rank.cpp
    src/services/dictionary/popup_entries.cpp
    src/services/dictionary/popup_html.cpp
    src/services/dictionary/popup_assets.cpp
    src/services/dictionary/text_selection.cpp
    src/services/reader/theme.cpp
    src/services/reader/book_session.cpp
    src/services/reader/chapter_remaining.cpp
    src/services/reader/toc_target.cpp
    src/services/reader/book_search.cpp
    src/services/reader/character_normalizer.cpp
    src/services/reader/layout.cpp
    src/services/reader/reader_css.cpp
    src/services/reader/reader_shell.cpp
    src/services/reader/reading_tracker.cpp
    src/models/statistics/dashboard_models.cpp
    src/models/statistics/reader_statistics.cpp
    src/models/books/book.cpp
    src/models/books/library_model.cpp
    src/services/books/book_storage.cpp
    src/services/books/library_watch.cpp
    src/services/books/library_watch.h
    src/models/books/book.h
    src/models/profiles/content_language.cpp
    src/models/profiles/dictionary_settings.cpp
    src/models/profiles/profile_index.cpp
    src/models/profiles/profile_index_json.cpp
    src/models/profiles/profile_ops.cpp
    src/models/profiles/reader_settings.cpp
    src/models/sasayaki/sasayaki.cpp
    src/models/statistics/statistics.cpp
    src/services/sasayaki/srt_parser.cpp
    src/services/sasayaki/cue_timeline.cpp
    src/services/sasayaki/matcher.cpp
    src/services/statistics/statistics_storage.cpp
    src/services/novels/epub_book.cpp
    src/services/player/mining_history.cpp
    src/services/player/mining_history_store.cpp
    src/services/player/playback_controls.cpp
    src/services/player/playback_history.cpp
    src/services/player/resume_options.cpp
    src/services/player/transcript_extractor.cpp
    src/services/player/transcript_extractor.h
    src/services/player/video_settings.cpp
    src/services/profiles/profile_repository.cpp
    src/services/settings/dictionary_settings_watch.cpp
    src/services/settings/dictionary_settings_watch.h
    src/services/settings/global_settings.cpp
    src/services/settings/settings_store.cpp
    src/services/storage/app_paths.cpp
    src/services/storage/json_file.cpp
)
qt_add_resources(niratan_core "dictionary_popup_assets"
    PREFIX "/niratan/dictionary_popup"
    BASE "src/web/dictionary_popup"
    FILES
        src/web/dictionary_popup/popup.css
        src/web/dictionary_popup/popup.js
        src/web/dictionary_popup/bridge.js
)
target_include_directories(niratan_core PUBLIC src)
target_link_libraries(niratan_core PUBLIC hoshidicts Qt6::Core Qt6::Network SQLite3::SQLite3 libzip::zip)
target_compile_options(niratan_core PRIVATE -Wall -Wextra -Wpedantic -Werror)

# The application layer above niratan_core: view models, view C++ types and
# the Niratan QML module. It is a static library so tests can link it.
qt_add_library(niratan_app STATIC
    src/viewmodels/dictionary/dictionary_popup_model.cpp
    src/viewmodels/dictionary/dictionary_popup_model.h
    src/viewmodels/library/library_view_model.cpp
    src/viewmodels/library/library_view_model.h
    src/viewmodels/profiles/profiles_view_model.cpp
    src/viewmodels/profiles/profiles_view_model.h
    src/viewmodels/sasayaki/sasayaki_view_model.cpp
    src/viewmodels/sasayaki/sasayaki_view_model.h
    src/viewmodels/statistics/statistics_view_model.cpp
    src/viewmodels/statistics/statistics_view_model.h
    src/viewmodels/player/player_study_view_model.cpp
    src/viewmodels/player/player_study_view_model.h
    src/viewmodels/settings/settings_view_model.cpp
    src/viewmodels/settings/settings_view_model.h
    src/viewmodels/reader/reader_view_model.cpp
    src/viewmodels/reader/reader_view_model_gallery.cpp
    src/viewmodels/reader/reader_view_model.h
    src/viewmodels/reader/reader_view_model_pagination.cpp
    src/viewmodels/reader/reader_view_model_state.h
    src/viewmodels/reader/reader_view_model_statistics.cpp
    src/views/player/mpv_item.cpp
    src/views/player/mpv_item.h
    src/views/player/mpv_playback_history.cpp
    src/views/player/mpv_subtitles.cpp
    src/views/player/mpv_renderer.cpp
    src/views/player/mpv_renderer.h
    src/views/reader/reader_scheme_handler.cpp
    src/views/reader/reader_scheme_handler.h
    src/views/dictionary/audio_scheme_handler.cpp
    src/views/dictionary/audio_scheme_handler.h
    src/views/dictionary/dictionary_media_scheme_handler.cpp
    src/views/dictionary/dictionary_media_scheme_handler.h
    src/views/theme/theme_state.cpp
    src/views/theme/theme_state.h
)
qt_add_resources(niratan_app "reader_scripts"
    PREFIX "/niratan/reader"
    BASE "src/web/reader"
    FILES
        src/web/reader/highlights.js
        src/web/reader/paragraph.js
        src/web/reader/reader.js
        src/web/reader/scrollreader.js
        src/web/reader/selection.js
)
set_source_files_properties(src/views/theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(niratan_app
    URI Niratan
    QML_FILES
        src/views/MainWindow.qml
        src/views/components/PageHeader.qml
        src/views/pages/DictionaryPage.qml
        src/views/pages/LibraryPage.qml
        src/views/pages/LibraryShelfSidebar.qml
        src/views/pages/PlayerPage.qml
        src/views/pages/ProfilesPage.qml
        src/views/pages/StatisticsPage.qml
        src/views/pages/SettingsPage.qml
        src/views/pages/SettingRow.qml
        src/views/pages/SettingsSliderRow.qml
        src/views/reader/SasayakiPanel.qml
        src/views/reader/ReaderStatisticsInspector.qml
        src/views/reader/ReaderProgress.qml
        src/views/reader/ReaderContents.qml
        src/views/reader/ReaderGallery.qml
        src/views/reader/ReaderChrome.qml
        src/views/reader/ReaderLookupSurface.qml
        src/views/player/BarButton.qml
        src/views/player/StudySidebar.qml
        src/views/player/StudyLoopMarker.qml
        src/views/player/StudyTimelineRow.qml
        src/views/player/VideoOpenMenu.qml
        src/views/player/VideoSettingsPanel.qml
        src/views/pages/ReaderPage.qml
        src/views/theme/Theme.qml
        src/views/theme/ThemePreview.qml
)
target_link_libraries(niratan_app PUBLIC niratan_core Qt6::Quick Qt6::QuickControls2
    Qt6::WebEngineQuick Qt6::WebChannelQuick Qt6::QuickDialogs2 Qt6::OpenGL
    Qt6::Multimedia PkgConfig::MPV)
target_include_directories(niratan_app PUBLIC src/views/theme src/viewmodels/dictionary
    src/viewmodels/reader src/views/reader src/viewmodels/library src/views/player
    src/viewmodels/profiles src/viewmodels/sasayaki src/viewmodels/statistics
    src/viewmodels/player src/viewmodels/settings)
target_compile_options(niratan_app PRIVATE -Wall -Wextra -Wpedantic -Werror)

# The QML module is static, so its generated plugin must be imported by the
# application for the module's types and resources to be registered.
qt_add_executable(niratan src/main.cpp)
target_link_libraries(niratan PRIVATE niratan_app niratan_appplugin)
target_compile_options(niratan PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_subdirectory(tests)
