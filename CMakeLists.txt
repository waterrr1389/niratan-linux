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

find_package(Qt6 6.12 REQUIRED COMPONENTS Quick QuickControls2 Test Network)
qt_standard_project_setup(REQUIRES 6.12)
find_package(SQLite3 REQUIRED)

qt_add_library(niratan_core STATIC
    src/models/dictionary/audio_source.cpp
    src/services/anki/field_mapping.cpp
    src/services/anki/duplicate_scope.cpp
    src/services/anki/duplicate_query.cpp
    src/services/anki/glossary_markup.cpp
    src/services/anki/media_naming.cpp
    src/services/anki/field_content.cpp
    src/services/anki/anki_connect.cpp
    src/services/anki/mining_gate.cpp
    src/services/audio/local_audio.cpp
    src/services/audio/local_file_server.cpp
    src/services/dictionary/dictionary_store.cpp
    src/services/dictionary/dictionary_query.cpp
    src/services/dictionary/frequency_rank.cpp
    src/services/dictionary/popup_entries.cpp
    src/services/dictionary/text_selection.cpp
    src/services/reader/theme.cpp
    src/services/reader/layout.cpp
    src/models/profiles/content_language.cpp
    src/models/profiles/profile_index.cpp
    src/services/storage/app_paths.cpp
    src/services/storage/json_file.cpp
)
target_include_directories(niratan_core PUBLIC src)
target_link_libraries(niratan_core PUBLIC hoshidicts Qt6::Core Qt6::Network SQLite3::SQLite3)
target_compile_options(niratan_core PRIVATE -Wall -Wextra -Wpedantic -Werror)

qt_add_executable(niratan
    src/main.cpp
    src/views/theme/theme_state.cpp
    src/views/theme/theme_state.h
)
set_source_files_properties(src/views/theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(niratan
    URI Niratan
    QML_FILES
        src/views/MainWindow.qml
        src/views/theme/Theme.qml
        src/views/theme/ThemePreview.qml
    RESOURCES
        src/web/dictionary_popup/popup.css
        src/web/dictionary_popup/popup.js
)
target_link_libraries(niratan PRIVATE niratan_core Qt6::Quick Qt6::QuickControls2)
target_include_directories(niratan PRIVATE src/views/theme)
target_compile_options(niratan PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_subdirectory(tests)
