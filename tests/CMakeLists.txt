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
