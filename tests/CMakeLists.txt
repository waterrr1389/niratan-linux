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
