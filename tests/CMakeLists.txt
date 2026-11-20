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

qt_add_executable(tst_mining_gate
    anki/tst_mining_gate.cpp
)
target_include_directories(tst_mining_gate PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_mining_gate PRIVATE niratan_core Qt6::Test)
target_compile_options(tst_mining_gate PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME mining_gate COMMAND tst_mining_gate)
