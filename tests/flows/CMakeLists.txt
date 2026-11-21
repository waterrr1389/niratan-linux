qt_add_executable(tst_flow_statistics
    tst_flow_statistics.cpp
)
target_include_directories(tst_flow_statistics PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../viewmodels
)
target_link_libraries(tst_flow_statistics PRIVATE niratan_app Qt6::Test)
target_compile_options(tst_flow_statistics PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME flow_statistics COMMAND tst_flow_statistics)
