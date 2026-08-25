include(GoogleTest)

set(PLOT_RUNTIME_DIR "${CMAKE_SOURCE_DIR}/src/runtime")

add_executable(unit_tests
  unit/test_diagnostics.cpp
  unit/test_lexer.cpp
  unit/test_parser.cpp
  unit/test_semantic.cpp
  unit/test_runtime.cpp
  unit/test_plot_font.cpp
  unit/test_png_encoder.cpp
  unit/test_plot_framebuffer.cpp
  unit/test_plot_mapping.cpp
  unit/test_plot_render.cpp
)
target_link_libraries(unit_tests PRIVATE mcdc_core GTest::gtest_main)
target_include_directories(unit_tests PRIVATE ${PLOT_RUNTIME_DIR})
gtest_discover_tests(unit_tests)

add_executable(golden_tests golden/test_golden_cooling.cpp golden/test_golden_plot.cpp)
target_link_libraries(golden_tests PRIVATE mcdc_core GTest::gtest_main)
target_include_directories(golden_tests PRIVATE ${PLOT_RUNTIME_DIR})
target_compile_definitions(golden_tests PRIVATE GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/golden")
gtest_discover_tests(golden_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(integration_tests
  integration/test_end_to_end.cpp
  integration/test_cli_errors.cpp
  integration/test_divergence.cpp
  integration/test_performance.cpp
  integration/test_plot_e2e.cpp
  integration/test_plot_columns.cpp
  integration/test_plot_errors.cpp
  integration/test_plot_perf.cpp
)
target_link_libraries(integration_tests PRIVATE mcdc_core GTest::gtest_main)
target_compile_definitions(integration_tests PRIVATE
  MODELICAC_BIN="$<TARGET_FILE:modelicac>"
  EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples/models"
  GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/golden"
)
gtest_discover_tests(integration_tests WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} TIMEOUT 120)

# 回归套件（US2）：omc 缺失时用例全部 SKIP，套件仍通过（run_regression 退出 0）。
add_test(
  NAME regression_suite
  COMMAND ${CMAKE_SOURCE_DIR}/tools/regression/run_regression.py
          ${CMAKE_SOURCE_DIR}/examples/models/references.cases
          --binary $<TARGET_FILE:modelicac>
)
