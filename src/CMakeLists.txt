add_library(mcdc_core STATIC
  diagnostics/diagnostic.cpp
  lexer/lexer.cpp
  ast/ast.cpp
  parser/parser.cpp
  semantic/symbols.cpp
  semantic/equations.cpp
  codegen/plan.cpp
  codegen/codegen.cpp
  cli/pipeline.cpp
)
target_include_directories(mcdc_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(mcdc_core PUBLIC
  MC_RUNTIME_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}/runtime"
)
target_compile_options(mcdc_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(modelicac cli/main.cpp)
target_link_libraries(modelicac PRIVATE mcdc_core)
target_compile_options(modelicac PRIVATE -Wall -Wextra -Wpedantic)
