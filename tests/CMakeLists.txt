qt_add_executable(tst_local_audio
    audio/tst_local_audio.cpp
)
target_include_directories(tst_local_audio PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_local_audio PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_local_audio PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME local_audio COMMAND tst_local_audio)

qt_add_executable(tst_local_file_server
    audio/tst_local_file_server.cpp
)
target_include_directories(tst_local_file_server PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_local_file_server PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_local_file_server PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME local_file_server COMMAND tst_local_file_server)

qt_add_executable(tst_dictionary
    dictionary/tst_dictionary.cpp
)
target_include_directories(tst_dictionary PRIVATE ${CMAKE_SOURCE_DIR}/src dictionary)
target_link_libraries(tst_dictionary PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_dictionary PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME dictionary COMMAND tst_dictionary)

qt_add_executable(tst_text_selection
    dictionary/tst_text_selection.cpp
)
target_include_directories(tst_text_selection PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_text_selection PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_text_selection PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME text_selection COMMAND tst_text_selection)

qt_add_executable(tst_frequency_rank
    dictionary/tst_frequency_rank.cpp
)
target_include_directories(tst_frequency_rank PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_frequency_rank PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_frequency_rank PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME frequency_rank COMMAND tst_frequency_rank)

qt_add_executable(tst_theme
    reader/tst_theme.cpp
)
target_include_directories(tst_theme PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_theme PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_theme PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME theme COMMAND tst_theme)

qt_add_executable(tst_layout
    reader/tst_layout.cpp
)
target_include_directories(tst_layout PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_layout PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_layout PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME layout COMMAND tst_layout)

qt_add_executable(tst_field_mapping
    anki/tst_field_mapping.cpp
)
target_include_directories(tst_field_mapping PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_field_mapping PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_field_mapping PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME field_mapping COMMAND tst_field_mapping)

qt_add_executable(tst_duplicate_scope
    anki/tst_duplicate_scope.cpp
)
target_include_directories(tst_duplicate_scope PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_duplicate_scope PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_duplicate_scope PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME duplicate_scope COMMAND tst_duplicate_scope)

qt_add_executable(tst_duplicate_query
    anki/tst_duplicate_query.cpp
)
target_include_directories(tst_duplicate_query PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_duplicate_query PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_duplicate_query PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME duplicate_query COMMAND tst_duplicate_query)

qt_add_executable(tst_glossary_markup
    anki/tst_glossary_markup.cpp
)
target_include_directories(tst_glossary_markup PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_glossary_markup PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_glossary_markup PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME glossary_markup COMMAND tst_glossary_markup)

qt_add_executable(tst_media_naming
    anki/tst_media_naming.cpp
)
target_include_directories(tst_media_naming PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_media_naming PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_media_naming PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME media_naming COMMAND tst_media_naming)

qt_add_executable(tst_field_content
    anki/tst_field_content.cpp
)
target_include_directories(tst_field_content PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_field_content PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_field_content PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME field_content COMMAND tst_field_content)

qt_add_executable(tst_anki_connect
    anki/tst_anki_connect.cpp
)
target_include_directories(tst_anki_connect PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_anki_connect PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_anki_connect PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME anki_connect COMMAND tst_anki_connect)

qt_add_executable(tst_mining_gate
    anki/tst_mining_gate.cpp
)
target_include_directories(tst_mining_gate PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_mining_gate PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_mining_gate PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME mining_gate COMMAND tst_mining_gate)

qt_add_executable(tst_audio_source
    audio/tst_audio_source.cpp
)
target_include_directories(tst_audio_source PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_audio_source PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_audio_source PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME audio_source COMMAND tst_audio_source)

qt_add_executable(tst_app_paths
    storage/tst_app_paths.cpp
)
target_include_directories(tst_app_paths PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_app_paths PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_app_paths PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME app_paths COMMAND tst_app_paths)

qt_add_executable(tst_json_file
    storage/tst_json_file.cpp
)
target_include_directories(tst_json_file PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_json_file PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_json_file PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME json_file COMMAND tst_json_file)

qt_add_executable(tst_content_language
    profiles/tst_content_language.cpp
)
target_include_directories(tst_content_language PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_content_language PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_content_language PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME content_language COMMAND tst_content_language)

qt_add_executable(tst_profile_index
    profiles/tst_profile_index.cpp
)
target_include_directories(tst_profile_index PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_profile_index PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_profile_index PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME profile_index COMMAND tst_profile_index)

qt_add_executable(tst_popup_entries
    dictionary/tst_popup_entries.cpp
)
target_include_directories(tst_popup_entries PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_popup_entries PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_popup_entries PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME popup_entries COMMAND tst_popup_entries)
