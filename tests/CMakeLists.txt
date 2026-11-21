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

qt_add_executable(tst_duplicate_lookup
    anki/tst_duplicate_lookup.cpp
)
target_include_directories(tst_duplicate_lookup PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_duplicate_lookup PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_duplicate_lookup PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME duplicate_lookup COMMAND tst_duplicate_lookup)

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

qt_add_executable(tst_transcript_extractor
    player/tst_transcript_extractor.cpp
)
target_include_directories(tst_transcript_extractor PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_transcript_extractor PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_transcript_extractor PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME transcript_extractor COMMAND tst_transcript_extractor)

qt_add_executable(tst_sasayaki
    sasayaki/tst_sasayaki.cpp
)
target_include_directories(tst_sasayaki PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_sasayaki PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_sasayaki PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME sasayaki COMMAND tst_sasayaki)

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

qt_add_executable(tst_matcher
    sasayaki/tst_matcher.cpp
)
target_include_directories(tst_matcher PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_matcher PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_matcher PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME matcher COMMAND tst_matcher)

qt_add_executable(tst_global_settings
    settings/tst_global_settings.cpp
)
target_include_directories(tst_global_settings PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_global_settings PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_global_settings PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME global_settings COMMAND tst_global_settings)

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
