qt_add_executable(tst_local_audio
    audio/tst_local_audio.cpp
)
target_include_directories(tst_local_audio PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_local_audio PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_local_audio PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME local_audio COMMAND tst_local_audio)

qt_add_executable(tst_web_audio_download
    audio/tst_web_audio_download.cpp
)
target_include_directories(tst_web_audio_download PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_web_audio_download PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_web_audio_download PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME web_audio_download COMMAND tst_web_audio_download)

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

qt_add_executable(tst_dictionary_config
    dictionary/tst_dictionary_config.cpp
)
target_include_directories(tst_dictionary_config PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_dictionary_config PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_dictionary_config PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME dictionary_config COMMAND tst_dictionary_config)

qt_add_executable(tst_dictionary_recommendation
    dictionary/tst_dictionary_recommendation.cpp
)
target_include_directories(tst_dictionary_recommendation PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_dictionary_recommendation PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_dictionary_recommendation PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME dictionary_recommendation COMMAND tst_dictionary_recommendation)

qt_add_executable(tst_lookup_configuration
    dictionary/tst_lookup_configuration.cpp
)
target_include_directories(tst_lookup_configuration PRIVATE ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/dictionary)
target_link_libraries(tst_lookup_configuration PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_lookup_configuration PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME lookup_configuration COMMAND tst_lookup_configuration)

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

qt_add_executable(tst_fetch_metadata
    anki/tst_fetch_metadata.cpp
)
target_include_directories(tst_fetch_metadata PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_fetch_metadata PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_fetch_metadata PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME fetch_metadata COMMAND tst_fetch_metadata)

qt_add_executable(tst_mapping_options
    anki/tst_mapping_options.cpp
)
target_include_directories(tst_mapping_options PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_mapping_options PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_mapping_options PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME mapping_options COMMAND tst_mapping_options)

qt_add_executable(tst_duplicate_query
    anki/tst_duplicate_query.cpp
)
target_include_directories(tst_duplicate_query PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_duplicate_query PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_duplicate_query PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME duplicate_query COMMAND tst_duplicate_query)

qt_add_executable(tst_duplicate_lookup
    anki/tst_duplicate_lookup.cpp
)
target_include_directories(tst_duplicate_lookup PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_duplicate_lookup PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_duplicate_lookup PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME duplicate_lookup COMMAND tst_duplicate_lookup)

qt_add_executable(tst_mining
    anki/tst_mining.cpp
)
target_include_directories(tst_mining PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_mining PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_mining PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME mining COMMAND tst_mining)

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

qt_add_executable(tst_popup_options
    anki/tst_popup_options.cpp
)
target_include_directories(tst_popup_options PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_popup_options PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_popup_options PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME popup_options COMMAND tst_popup_options)

qt_add_executable(tst_anki_connect
    anki/tst_anki_connect.cpp
)
target_include_directories(tst_anki_connect PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_anki_connect PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_anki_connect PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME anki_connect COMMAND tst_anki_connect)

qt_add_executable(tst_anki_requests
    anki/tst_anki_requests.cpp
)
target_include_directories(tst_anki_requests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_anki_requests PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_anki_requests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME anki_requests COMMAND tst_anki_requests)

qt_add_executable(tst_mining_gate
    anki/tst_mining_gate.cpp
)
target_include_directories(tst_mining_gate PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_mining_gate PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_mining_gate PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME mining_gate COMMAND tst_mining_gate)

qt_add_executable(tst_anki_config
    anki/tst_anki_config.cpp
)
target_include_directories(tst_anki_config PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_anki_config PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_anki_config PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME anki_config COMMAND tst_anki_config)

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

qt_add_executable(tst_popup_html
    dictionary/tst_popup_html.cpp
)
target_include_directories(tst_popup_html PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_popup_html PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_popup_html PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME popup_html COMMAND tst_popup_html)

qt_add_executable(tst_popup_assets
    dictionary/tst_popup_assets.cpp
)
target_include_directories(tst_popup_assets PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_popup_assets PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_popup_assets PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME popup_assets COMMAND tst_popup_assets)

qt_add_executable(tst_media_type
    dictionary/tst_media_type.cpp
)
target_include_directories(tst_media_type PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_media_type PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_media_type PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME media_type COMMAND tst_media_type)

qt_add_executable(tst_term_dictionaries
    dictionary/tst_term_dictionaries.cpp
)
target_include_directories(tst_term_dictionaries PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_term_dictionaries PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_term_dictionaries PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME term_dictionaries COMMAND tst_term_dictionaries)

qt_add_executable(tst_audio_request
    audio/tst_audio_request.cpp
)
target_include_directories(tst_audio_request PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_audio_request PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_audio_request PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME audio_request COMMAND tst_audio_request)

qt_add_executable(tst_profile_ops
    profiles/tst_profile_ops.cpp
)
target_include_directories(tst_profile_ops PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_profile_ops PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_profile_ops PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME profile_ops COMMAND tst_profile_ops)

qt_add_executable(tst_profile_index_json
    profiles/tst_profile_index_json.cpp
)
target_include_directories(tst_profile_index_json PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_profile_index_json PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_profile_index_json PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME profile_index_json COMMAND tst_profile_index_json)

qt_add_executable(tst_profile_repository
    profiles/tst_profile_repository.cpp
)
target_include_directories(tst_profile_repository PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_profile_repository PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_profile_repository PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME profile_repository COMMAND tst_profile_repository)

qt_add_executable(tst_epub_book
    novels/tst_epub_book.cpp
)
target_include_directories(tst_epub_book PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_epub_book PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_epub_book PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME epub_book COMMAND tst_epub_book)

qt_add_executable(tst_epub_manifest novels/tst_epub_manifest.cpp)
target_include_directories(tst_epub_manifest PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(tst_epub_manifest PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_epub_manifest PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME epub_manifest COMMAND tst_epub_manifest)

qt_add_executable(tst_reader_settings
    profiles/tst_reader_settings.cpp
)
target_include_directories(tst_reader_settings PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_reader_settings PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_reader_settings PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME reader_settings COMMAND tst_reader_settings)

qt_add_executable(tst_book_model
    books/tst_book_model.cpp
)
target_include_directories(tst_book_model PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_book_model PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_book_model PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME book_model COMMAND tst_book_model)

qt_add_executable(tst_reader_css
    reader/tst_reader_css.cpp
)
target_include_directories(tst_reader_css PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_reader_css PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_reader_css PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME reader_css COMMAND tst_reader_css)

qt_add_executable(tst_character_normalizer
    reader/tst_character_normalizer.cpp
)
target_include_directories(tst_character_normalizer PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_character_normalizer PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_character_normalizer PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME character_normalizer COMMAND tst_character_normalizer)

qt_add_executable(tst_reader_shell
    reader/tst_reader_shell.cpp
)
target_include_directories(tst_reader_shell PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_reader_shell PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_reader_shell PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME reader_shell COMMAND tst_reader_shell)

qt_add_executable(tst_document_context
    reader/tst_document_context.cpp
)
target_link_libraries(tst_document_context PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_document_context PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME document_context COMMAND tst_document_context)

qt_add_executable(tst_book_search
    reader/tst_book_search.cpp
)
target_include_directories(tst_book_search PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_book_search PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_book_search PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME book_search COMMAND tst_book_search)

qt_add_executable(tst_book_session
    reader/tst_book_session.cpp
)
target_include_directories(tst_book_session PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_book_session PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_book_session PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME book_session COMMAND tst_book_session)

qt_add_executable(tst_book_storage
    books/tst_book_storage.cpp
)
target_include_directories(tst_book_storage PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_book_storage PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_book_storage PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME book_storage COMMAND tst_book_storage)

qt_add_executable(tst_book_export
    books/tst_book_export.cpp
)
target_include_directories(tst_book_export PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_book_export PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_book_export PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME book_export COMMAND tst_book_export)

qt_add_executable(tst_library_watch
    books/tst_library_watch.cpp
)
target_include_directories(tst_library_watch PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_library_watch PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_library_watch PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME library_watch COMMAND tst_library_watch)

qt_add_executable(tst_statistics
    statistics/tst_statistics.cpp
)
target_include_directories(tst_statistics PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_statistics PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_statistics PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME statistics COMMAND tst_statistics)

qt_add_executable(tst_ttu_naming sync/tst_ttu_naming.cpp)
target_include_directories(tst_ttu_naming PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_ttu_naming PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_ttu_naming PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ttu_naming COMMAND tst_ttu_naming)

qt_add_executable(tst_drive_client sync/tst_drive_client.cpp fakes/fake_drive_server.cpp)
target_include_directories(tst_drive_client PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(tst_drive_client PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_drive_client PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME drive_client COMMAND tst_drive_client)

qt_add_executable(tst_ttu_sync sync/tst_ttu_sync.cpp fakes/fake_drive_server.cpp)
target_include_directories(tst_ttu_sync PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(tst_ttu_sync PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_ttu_sync PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ttu_sync COMMAND tst_ttu_sync)

qt_add_executable(tst_ttu_convert sync/tst_ttu_convert.cpp)
target_include_directories(tst_ttu_convert PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(tst_ttu_convert PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_ttu_convert PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ttu_convert COMMAND tst_ttu_convert)

qt_add_executable(tst_google_drive_auth sync/tst_google_drive_auth.cpp fakes/fake_drive_server.cpp)
target_include_directories(tst_google_drive_auth PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(tst_google_drive_auth PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_google_drive_auth PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME google_drive_auth COMMAND tst_google_drive_auth)

qt_add_executable(tst_flow_sync flows/tst_flow_sync.cpp fakes/fake_drive_server.cpp)
target_include_directories(tst_flow_sync PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(tst_flow_sync PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_flow_sync PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME flow_sync COMMAND tst_flow_sync)

qt_add_executable(tst_library_model
    books/tst_library_model.cpp
)
target_include_directories(tst_library_model PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_library_model PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_library_model PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME library_model COMMAND tst_library_model)

qt_add_executable(tst_dictionary_settings
    profiles/tst_dictionary_settings.cpp
)
target_include_directories(tst_dictionary_settings PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_dictionary_settings PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_dictionary_settings PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME dictionary_settings COMMAND tst_dictionary_settings)

qt_add_executable(tst_audio_clip
    player/tst_audio_clip.cpp
)
target_include_directories(tst_audio_clip PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_audio_clip PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_audio_clip PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME audio_clip COMMAND tst_audio_clip)

qt_add_executable(tst_mining_history
    player/tst_mining_history.cpp
)
target_include_directories(tst_mining_history PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_mining_history PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_mining_history PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME mining_history COMMAND tst_mining_history)

qt_add_executable(tst_mining_history_store
    player/tst_mining_history_store.cpp
)
target_include_directories(tst_mining_history_store PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_mining_history_store PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_mining_history_store PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME mining_history_store COMMAND tst_mining_history_store)

qt_add_executable(tst_playback_controls
    player/tst_playback_controls.cpp
)
target_include_directories(tst_playback_controls PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_playback_controls PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_playback_controls PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME playback_controls COMMAND tst_playback_controls)

qt_add_executable(tst_video_settings
    player/tst_video_settings.cpp
)
target_include_directories(tst_video_settings PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_video_settings PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_video_settings PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME video_settings COMMAND tst_video_settings)

qt_add_executable(tst_subtitle_gap_playback
    player/tst_subtitle_gap_playback.cpp
)
target_include_directories(tst_subtitle_gap_playback PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_subtitle_gap_playback PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_subtitle_gap_playback PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME subtitle_gap_playback COMMAND tst_subtitle_gap_playback)

qt_add_executable(tst_video_study_search
    player/tst_video_study_search.cpp
)
target_include_directories(tst_video_study_search PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_video_study_search PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_video_study_search PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME video_study_search COMMAND tst_video_study_search)

qt_add_executable(tst_library_catalog
    video/tst_library_catalog.cpp
)
target_include_directories(tst_library_catalog PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_library_catalog PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_library_catalog PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME library_catalog COMMAND tst_library_catalog)

qt_add_executable(tst_video_thumbnail_store
    video/tst_video_thumbnail_store.cpp
)
target_include_directories(tst_video_thumbnail_store PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_video_thumbnail_store PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_video_thumbnail_store PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME video_thumbnail_store COMMAND tst_video_thumbnail_store)

qt_add_executable(tst_library_view_model
    viewmodels/tst_library_view_model.cpp
)
target_include_directories(tst_library_view_model PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_library_view_model PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_library_view_model PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME library_view_model COMMAND tst_library_view_model)

qt_add_executable(tst_video_inspector
    qml/tst_video_inspector.cpp
)
target_include_directories(tst_video_inspector PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/viewmodels
)
target_link_libraries(tst_video_inspector PRIVATE niratan_app niratan_appplugin niratan_style
    niratan_styleplugin Qt6::Test
    Qt6::Quick Qt6::QuickControls2 Qt6::WebEngineQuick)
target_compile_options(tst_video_inspector PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME video_inspector COMMAND tst_video_inspector)
set_tests_properties(video_inspector PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_video_library_view_model
    viewmodels/tst_video_library_view_model.cpp
)
target_include_directories(tst_video_library_view_model PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_video_library_view_model PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_video_library_view_model PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME video_library_view_model COMMAND tst_video_library_view_model)

qt_add_executable(tst_playback_history
    player/tst_playback_history.cpp
)
target_include_directories(tst_playback_history PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_playback_history PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_playback_history PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME playback_history COMMAND tst_playback_history)

qt_add_executable(tst_resume_options player/tst_resume_options.cpp)
target_include_directories(tst_resume_options PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_resume_options PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_resume_options PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME resume_options COMMAND tst_resume_options)

qt_add_executable(tst_subtitle_default
    player/tst_subtitle_default.cpp
)
target_include_directories(tst_subtitle_default PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_subtitle_default PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_subtitle_default PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME subtitle_default COMMAND tst_subtitle_default)

qt_add_executable(tst_subtitle_document
    player/tst_subtitle_document.cpp
)
target_include_directories(tst_subtitle_document PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_subtitle_document PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_subtitle_document PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME subtitle_document COMMAND tst_subtitle_document)

qt_add_executable(tst_ass_subtitle
    player/tst_ass_subtitle.cpp
)
target_include_directories(tst_ass_subtitle PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_ass_subtitle PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_ass_subtitle PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ass_subtitle COMMAND tst_ass_subtitle)

qt_add_executable(tst_transcript_extractor
    player/tst_transcript_extractor.cpp
)
target_include_directories(tst_transcript_extractor PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_transcript_extractor PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_transcript_extractor PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME transcript_extractor COMMAND tst_transcript_extractor)

qt_add_executable(tst_video_mining
    player/tst_video_mining.cpp
)
target_include_directories(tst_video_mining PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_video_mining PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_video_mining PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME video_mining COMMAND tst_video_mining)

qt_add_executable(tst_sasayaki
    sasayaki/tst_sasayaki.cpp
)
target_include_directories(tst_sasayaki PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_sasayaki PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_sasayaki PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME sasayaki COMMAND tst_sasayaki)

qt_add_executable(tst_sasayaki_settings
    sasayaki/tst_sasayaki_settings.cpp
)
target_include_directories(tst_sasayaki_settings PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_sasayaki_settings PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_sasayaki_settings PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME sasayaki_settings COMMAND tst_sasayaki_settings)

qt_add_executable(tst_audiobook_metadata
    audiobook/tst_audiobook_metadata.cpp
)
target_include_directories(tst_audiobook_metadata PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_audiobook_metadata PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_audiobook_metadata PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME audiobook_metadata COMMAND tst_audiobook_metadata)

qt_add_executable(tst_statistics_storage
    statistics/tst_statistics_storage.cpp
)
target_include_directories(tst_statistics_storage PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_statistics_storage PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_statistics_storage PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME statistics_storage COMMAND tst_statistics_storage)

qt_add_executable(tst_reading_tracker
    reader/tst_reading_tracker.cpp
)
target_include_directories(tst_reading_tracker PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_reading_tracker PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_reading_tracker PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME reading_tracker COMMAND tst_reading_tracker)

qt_add_executable(tst_reader_statistics
    reader/tst_reader_statistics.cpp
)
target_include_directories(tst_reader_statistics PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_reader_statistics PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_reader_statistics PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME reader_statistics COMMAND tst_reader_statistics)

qt_add_executable(tst_dashboard_models
    statistics/tst_dashboard_models.cpp
)
target_include_directories(tst_dashboard_models PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_dashboard_models PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_dashboard_models PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME dashboard_models COMMAND tst_dashboard_models)

qt_add_executable(tst_cue_timeline
    sasayaki/tst_cue_timeline.cpp
)
target_include_directories(tst_cue_timeline PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_cue_timeline PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_cue_timeline PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME cue_timeline COMMAND tst_cue_timeline)

qt_add_executable(tst_cue_audio
    sasayaki/tst_cue_audio.cpp
)
target_include_directories(tst_cue_audio PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_cue_audio PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_cue_audio PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME cue_audio COMMAND tst_cue_audio)

qt_add_executable(tst_matcher
    sasayaki/tst_matcher.cpp
)
target_include_directories(tst_matcher PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_matcher PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_matcher PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME matcher COMMAND tst_matcher)

qt_add_executable(tst_match_chapters
    sasayaki/tst_match_chapters.cpp
)
target_include_directories(tst_match_chapters PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_match_chapters PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_match_chapters PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME match_chapters COMMAND tst_match_chapters)

qt_add_executable(tst_global_settings
    settings/tst_global_settings.cpp
)
target_include_directories(tst_global_settings PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_global_settings PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_global_settings PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME global_settings COMMAND tst_global_settings)

qt_add_executable(tst_sync_settings_view_model
    viewmodels/tst_sync_settings_view_model.cpp
    fakes/fake_drive_server.cpp
)
target_include_directories(tst_sync_settings_view_model PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(tst_sync_settings_view_model PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_sync_settings_view_model PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME sync_settings_view_model COMMAND tst_sync_settings_view_model)

qt_add_executable(tst_settings_store
    settings/tst_settings_store.cpp
)
target_include_directories(tst_settings_store PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_settings_store PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_settings_store PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME settings_store COMMAND tst_settings_store)

qt_add_executable(tst_dictionary_settings_watch
    settings/tst_dictionary_settings_watch.cpp
)
target_include_directories(tst_dictionary_settings_watch PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_dictionary_settings_watch PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_dictionary_settings_watch PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME dictionary_settings_watch COMMAND tst_dictionary_settings_watch)

# Application-layer tests. View models build their services internally and
# read the data root from the environment, so these run with HOME and the XDG
# directories pointed at a temporary directory and with Qt offscreen.

qt_add_executable(tst_mining_view_model
    viewmodels/tst_mining_view_model.cpp
)
target_include_directories(tst_mining_view_model PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_mining_view_model PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_mining_view_model PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME mining_view_model COMMAND tst_mining_view_model)

qt_add_executable(tst_sasayaki_view_model
    viewmodels/tst_sasayaki_view_model.cpp
)
target_include_directories(tst_sasayaki_view_model PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_sasayaki_view_model PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_sasayaki_view_model PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME sasayaki_view_model COMMAND tst_sasayaki_view_model)

qt_add_executable(tst_sasayaki_settings_view_model
    viewmodels/tst_sasayaki_settings_view_model.cpp
)
target_include_directories(tst_sasayaki_settings_view_model PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_sasayaki_settings_view_model PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_sasayaki_settings_view_model PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME sasayaki_settings_view_model COMMAND tst_sasayaki_settings_view_model)

qt_add_executable(tst_anki_settings_view_model
    viewmodels/tst_anki_settings_view_model.cpp
)
target_include_directories(tst_anki_settings_view_model PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_anki_settings_view_model PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_anki_settings_view_model PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME anki_settings_view_model COMMAND tst_anki_settings_view_model)

qt_add_executable(tst_profiles_view_model
    viewmodels/tst_profiles_view_model.cpp
)
target_include_directories(tst_profiles_view_model PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_profiles_view_model PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_profiles_view_model PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME profiles_view_model COMMAND tst_profiles_view_model)

qt_add_executable(tst_settings_view_model
    viewmodels/tst_settings_view_model.cpp
)
target_include_directories(tst_settings_view_model PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_settings_view_model PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_settings_view_model PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME settings_view_model COMMAND tst_settings_view_model)

qt_add_executable(tst_reader_font_size
    viewmodels/tst_reader_font_size.cpp
)
target_include_directories(tst_reader_font_size PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_reader_font_size PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_reader_font_size PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME reader_font_size COMMAND tst_reader_font_size)

qt_add_executable(tst_dictionary_settings_view_model
    viewmodels/tst_dictionary_settings_view_model.cpp
)
target_include_directories(tst_dictionary_settings_view_model PRIVATE ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/dictionary)
target_link_libraries(tst_dictionary_settings_view_model PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_dictionary_settings_view_model PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME dictionary_settings_view_model COMMAND tst_dictionary_settings_view_model)

qt_add_executable(tst_dictionary_popup_model
    viewmodels/tst_dictionary_popup_model.cpp
)
target_include_directories(tst_dictionary_popup_model PRIVATE ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/dictionary)
target_link_libraries(tst_dictionary_popup_model PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_dictionary_popup_model PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME dictionary_popup_model COMMAND tst_dictionary_popup_model)
set_tests_properties(dictionary_popup_model PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_player_lookup_view_model
    viewmodels/tst_player_lookup_view_model.cpp
)
target_include_directories(tst_player_lookup_view_model PRIVATE ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/dictionary)
target_link_libraries(tst_player_lookup_view_model PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_player_lookup_view_model PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME player_lookup_view_model COMMAND tst_player_lookup_view_model)
set_tests_properties(player_lookup_view_model PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_player_study_view_model
    viewmodels/tst_player_study_view_model.cpp
)
target_include_directories(tst_player_study_view_model PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_player_study_view_model PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_player_study_view_model PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME player_study_view_model COMMAND tst_player_study_view_model)

qt_add_executable(tst_player_inspector_view_model
    viewmodels/tst_player_inspector_view_model.cpp
)
target_include_directories(tst_player_inspector_view_model PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_player_inspector_view_model PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_player_inspector_view_model PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME player_inspector_view_model COMMAND tst_player_inspector_view_model)

qt_add_executable(tst_reader_mining_context
    viewmodels/tst_reader_mining_context.cpp
)
target_link_libraries(tst_reader_mining_context PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_reader_mining_context PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME reader_mining_context COMMAND tst_reader_mining_context)

qt_add_executable(tst_reader_sasayaki_navigation
    viewmodels/tst_reader_sasayaki_navigation.cpp
)
target_link_libraries(tst_reader_sasayaki_navigation PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_reader_sasayaki_navigation PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME reader_sasayaki_navigation COMMAND tst_reader_sasayaki_navigation)

qt_add_executable(tst_reader_navigation_history
    viewmodels/tst_reader_navigation_history.cpp
)
target_link_libraries(tst_reader_navigation_history PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_reader_navigation_history PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME reader_navigation_history COMMAND tst_reader_navigation_history)

qt_add_executable(tst_reader_open_sync
    viewmodels/tst_reader_open_sync.cpp
    fakes/fake_drive_server.cpp
)
target_include_directories(tst_reader_open_sync PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(tst_reader_open_sync PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_reader_open_sync PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME reader_open_sync COMMAND tst_reader_open_sync)

qt_add_executable(tst_reader_presentation_text
    viewmodels/tst_reader_presentation_text.cpp
)
target_include_directories(tst_reader_presentation_text PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_reader_presentation_text PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_reader_presentation_text PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME reader_presentation_text COMMAND tst_reader_presentation_text)

qt_add_executable(tst_qml_load
    qml/tst_qml_load.cpp
)
target_include_directories(tst_qml_load PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_qml_load PRIVATE niratan_app niratan_appplugin niratan_style
    niratan_styleplugin Qt6::Test
    Qt6::Quick Qt6::QuickControls2 Qt6::WebEngineQuick)
target_compile_options(tst_qml_load PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME qml_load COMMAND tst_qml_load)
set_tests_properties(qml_load PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_library_drag
    qml/tst_library_drag.cpp
)
target_include_directories(tst_library_drag PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_library_drag PRIVATE niratan_app niratan_appplugin niratan_style
    niratan_styleplugin Qt6::Test
    Qt6::Quick Qt6::QuickControls2 Qt6::WebEngineQuick)
target_compile_options(tst_library_drag PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME library_drag COMMAND tst_library_drag)
set_tests_properties(library_drag PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_library_import_progress
    qml/tst_library_import_progress.cpp
)
target_include_directories(tst_library_import_progress PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_library_import_progress PRIVATE niratan_app niratan_appplugin
    niratan_style niratan_styleplugin Qt6::Test
    Qt6::Quick Qt6::QuickControls2 Qt6::WebEngineQuick)
target_compile_options(tst_library_import_progress PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME library_import_progress COMMAND tst_library_import_progress)
set_tests_properties(library_import_progress PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_lookup_audio_menu
    qml/tst_lookup_audio_menu.cpp
)
target_include_directories(tst_lookup_audio_menu PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_lookup_audio_menu PRIVATE niratan_app niratan_appplugin niratan_style
    niratan_styleplugin Qt6::Test
    Qt6::Quick Qt6::QuickControls2 Qt6::WebEngineQuick)
target_compile_options(tst_lookup_audio_menu PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME lookup_audio_menu COMMAND tst_lookup_audio_menu)
set_tests_properties(lookup_audio_menu PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_settings_appearance
    qml/tst_settings_appearance.cpp
)
target_include_directories(tst_settings_appearance PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_settings_appearance PRIVATE niratan_app niratan_appplugin niratan_style
    niratan_styleplugin Qt6::Test
    Qt6::Quick Qt6::QuickControls2 Qt6::WebEngineQuick)
target_compile_options(tst_settings_appearance PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME settings_appearance COMMAND tst_settings_appearance)
set_tests_properties(settings_appearance PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_settings_sync qml/tst_settings_sync.cpp)
target_include_directories(tst_settings_sync PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_settings_sync PRIVATE niratan_app niratan_appplugin niratan_style
    niratan_styleplugin Qt6::Test Qt6::Quick Qt6::QuickControls2 Qt6::WebEngineQuick)
target_compile_options(tst_settings_sync PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME settings_sync COMMAND tst_settings_sync)
# The offscreen platform turns control hover off; the book menu test hovers.
set_tests_properties(settings_sync PROPERTIES
    ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu;QT_QUICK_CONTROLS_HOVER_ENABLED=1")

qt_add_executable(tst_control_style
    qml/tst_control_style.cpp
)
target_include_directories(tst_control_style PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_control_style PRIVATE niratan_app niratan_appplugin niratan_style
    niratan_styleplugin Qt6::Test Qt6::Quick Qt6::QuickControls2 Qt6::WebEngineQuick)
target_compile_options(tst_control_style PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME control_style COMMAND tst_control_style)
set_tests_properties(control_style PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_dictionary_reorder
    qml/tst_dictionary_reorder.cpp
)
target_include_directories(tst_dictionary_reorder PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/viewmodels
)
target_link_libraries(tst_dictionary_reorder PRIVATE niratan_app niratan_appplugin niratan_style
    niratan_styleplugin Qt6::Test Qt6::Quick Qt6::QuickControls2)
target_compile_options(tst_dictionary_reorder PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME dictionary_reorder COMMAND tst_dictionary_reorder)
set_tests_properties(dictionary_reorder PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_video_lookup_focus
    qml/tst_video_lookup_focus.cpp
)
target_include_directories(tst_video_lookup_focus PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/viewmodels
)
target_link_libraries(tst_video_lookup_focus PRIVATE niratan_app niratan_appplugin niratan_style
    niratan_styleplugin Qt6::Test
    Qt6::Quick Qt6::QuickControls2 Qt6::WebEngineQuick)
target_compile_options(tst_video_lookup_focus PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME video_lookup_focus COMMAND tst_video_lookup_focus)
set_tests_properties(video_lookup_focus PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_video_subtitle_lookup
    qml/tst_video_subtitle_lookup.cpp
)
target_include_directories(tst_video_subtitle_lookup PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_video_subtitle_lookup PRIVATE niratan_app niratan_appplugin
    niratan_style niratan_styleplugin Qt6::Test Qt6::Quick Qt6::QuickControls2)
target_compile_options(tst_video_subtitle_lookup PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME video_subtitle_lookup COMMAND tst_video_subtitle_lookup)
set_tests_properties(video_subtitle_lookup PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_control_kit
    qml/tst_control_kit.cpp
)
target_include_directories(tst_control_kit PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_control_kit PRIVATE niratan_app niratan_appplugin niratan_style
    niratan_styleplugin Qt6::Test Qt6::Quick Qt6::QuickControls2)
target_compile_options(tst_control_kit PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME control_kit COMMAND tst_control_kit)
set_tests_properties(control_kit PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_dictionary_view
    qml/tst_dictionary_view.cpp
)
target_include_directories(tst_dictionary_view PRIVATE ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests/dictionary)
target_link_libraries(tst_dictionary_view PRIVATE niratan_app niratan_appplugin niratan_style
    niratan_styleplugin Qt6::Test
    Qt6::Quick Qt6::QuickControls2 Qt6::WebEngineQuick)
target_compile_options(tst_dictionary_view PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME dictionary_view COMMAND tst_dictionary_view)
set_tests_properties(dictionary_view PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_video_window
    qml/tst_video_window.cpp
)
target_include_directories(tst_video_window PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/viewmodels
)
target_link_libraries(tst_video_window PRIVATE niratan_app niratan_appplugin niratan_style
    niratan_styleplugin Qt6::Test
    Qt6::Quick Qt6::QuickControls2 Qt6::WebEngineQuick)
target_compile_options(tst_video_window PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME video_window COMMAND tst_video_window)
set_tests_properties(video_window PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_player_controls
    qml/tst_player_controls.cpp
)
target_include_directories(tst_player_controls PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/viewmodels
)
target_link_libraries(tst_player_controls PRIVATE niratan_app niratan_appplugin niratan_style
    niratan_styleplugin Qt6::Test
    Qt6::Quick Qt6::QuickControls2 Qt6::WebEngineQuick)
target_compile_options(tst_player_controls PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME player_controls COMMAND tst_player_controls)
set_tests_properties(player_controls PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_video_library_view
    qml/tst_video_library_view.cpp
)
target_include_directories(tst_video_library_view PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/viewmodels
)
target_link_libraries(tst_video_library_view PRIVATE niratan_app niratan_appplugin niratan_style
    niratan_styleplugin Qt6::Test
    Qt6::Quick Qt6::QuickControls2 Qt6::WebEngineQuick)
target_compile_options(tst_video_library_view PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME video_library_view COMMAND tst_video_library_view)
set_tests_properties(video_library_view PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_statistics_view
    qml/tst_statistics_view.cpp
)
target_include_directories(tst_statistics_view PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/viewmodels
)
target_link_libraries(tst_statistics_view PRIVATE niratan_app niratan_appplugin niratan_style
    niratan_styleplugin Qt6::Test
    Qt6::Quick Qt6::QuickControls2 Qt6::WebEngineQuick)
target_compile_options(tst_statistics_view PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME statistics_view COMMAND tst_statistics_view)
set_tests_properties(statistics_view PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_interface_theme
    qml/tst_interface_theme.cpp
)
target_include_directories(tst_interface_theme PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/viewmodels
)
target_link_libraries(tst_interface_theme PRIVATE niratan_app niratan_appplugin niratan_style
    niratan_styleplugin Qt6::Test
    Qt6::Quick Qt6::QuickControls2 Qt6::WebEngineQuick)
target_compile_options(tst_interface_theme PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME interface_theme COMMAND tst_interface_theme)
set_tests_properties(interface_theme PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_reader_presentation
    qml/tst_reader_presentation.cpp
)
target_include_directories(tst_reader_presentation PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/viewmodels
    ${CMAKE_CURRENT_SOURCE_DIR}/flows
    ${CMAKE_CURRENT_SOURCE_DIR}/dictionary
)
target_link_libraries(tst_reader_presentation PRIVATE niratan_app niratan_appplugin niratan_style
    niratan_styleplugin Qt6::Test
    Qt6::Quick Qt6::QuickControls2 Qt6::WebEngineQuick)
target_compile_options(tst_reader_presentation PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME reader_presentation COMMAND tst_reader_presentation)
set_tests_properties(reader_presentation PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_reader_auto_sync
    qml/tst_reader_auto_sync.cpp
)
target_include_directories(tst_reader_auto_sync PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/viewmodels
    ${CMAKE_CURRENT_SOURCE_DIR}/flows
)
target_link_libraries(tst_reader_auto_sync PRIVATE niratan_app niratan_appplugin niratan_style
    niratan_styleplugin Qt6::Test
    Qt6::Quick Qt6::QuickControls2 Qt6::WebEngineQuick)
target_compile_options(tst_reader_auto_sync PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME reader_auto_sync COMMAND tst_reader_auto_sync)
set_tests_properties(reader_auto_sync PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

find_package(Qt6 REQUIRED COMPONENTS WebEngineWidgets)

qt_add_executable(tst_reader_images
    web/tst_reader_images.cpp
)
target_include_directories(tst_reader_images PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_reader_images PRIVATE niratan_app Qt6::Test Qt6::WebEngineWidgets
    Qt6::WebChannelQuick)
target_compile_options(tst_reader_images PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME reader_images COMMAND tst_reader_images)
set_tests_properties(reader_images PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_popup_bridge
    web/tst_popup_bridge.cpp
)
target_include_directories(tst_popup_bridge PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_popup_bridge PRIVATE niratan_core Qt6::Test Qt6::WebEngineQuick
    Qt6::WebChannelQuick)
target_compile_options(tst_popup_bridge PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME popup_bridge COMMAND tst_popup_bridge)
set_tests_properties(popup_bridge PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_reader_selection
    web/tst_reader_selection.cpp
)
target_link_libraries(tst_reader_selection PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_reader_selection PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME reader_selection COMMAND tst_reader_selection)
set_tests_properties(reader_selection PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

# Demo data for offscreen renders of the views; built, never run by ctest.
qt_add_executable(niratan_demo_seed
    demo/demo_seed.cpp
)
target_include_directories(niratan_demo_seed PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(niratan_demo_seed PRIVATE niratan_core Qt6::Gui)
target_compile_options(niratan_demo_seed PRIVATE -Wall -Wextra -Wpedantic -Werror)

# Flow tests: one per first-release flow, driving view models and services from
# a user action to its result with fixtures (EPUB, dictionary, AnkiConnect).
# Put them under tests/flows and add them here; scripts/check-wired.sh treats
# that directory as wiring roots.
add_subdirectory(flows)
