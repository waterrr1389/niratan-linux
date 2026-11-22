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

qt_add_executable(tst_flow_reader_mining
    tst_flow_reader_mining.cpp
)
target_include_directories(tst_flow_reader_mining PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../viewmodels
    ${CMAKE_CURRENT_SOURCE_DIR}/../dictionary
)
target_link_libraries(tst_flow_reader_mining PRIVATE niratan_app Qt6::Test
    Qt6::WebEngineQuick Qt6::WebChannelQuick)
target_compile_options(tst_flow_reader_mining PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME flow_reader_mining COMMAND tst_flow_reader_mining)
set_tests_properties(flow_reader_mining PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")

qt_add_executable(tst_flow_video_mining
    tst_flow_video_mining.cpp
)
target_include_directories(tst_flow_video_mining PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../viewmodels
    ${CMAKE_CURRENT_SOURCE_DIR}/../dictionary
)
target_link_libraries(tst_flow_video_mining PRIVATE niratan_app Qt6::Test
    Qt6::WebEngineQuick Qt6::WebChannelQuick)
target_compile_options(tst_flow_video_mining PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME flow_video_mining COMMAND tst_flow_video_mining)
set_tests_properties(flow_video_mining PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")
