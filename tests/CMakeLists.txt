if(GREVIR_BUILD_COMPILE_CHECKS)
  file(GLOB_RECURSE public_headers CONFIGURE_DEPENDS RELATIVE
    "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/*.hpp" "${PROJECT_SOURCE_DIR}/src/*.h")
  set(header_sources)
  foreach(header IN LISTS public_headers)
    string(MAKE_C_IDENTIFIER "${header}" identifier)
    set(source "${CMAKE_CURRENT_BINARY_DIR}/${identifier}.cpp")
    file(WRITE "${source}" "#include <${header}>\n")
    list(APPEND header_sources "${source}")
  endforeach()
  add_library(grevir_registers_compile OBJECT extracted_static_tests.cpp mapping_static_tests.cpp ${header_sources})
  target_link_libraries(grevir_registers_compile PRIVATE grevir::registers)
  set_target_properties(grevir_registers_compile PROPERTIES CXX_EXTENSIONS OFF)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$")
    message(FATAL_ERROR "Register contract probes currently require a Clang/GNU driver")
  endif()
  add_custom_target(grevir_registers_contract_checks ALL
    COMMAND "${CMAKE_COMMAND}"
      "-DCXX=${CMAKE_CXX_COMPILER}"
      "-DINCLUDE_DIRS=$<TARGET_PROPERTY:grevir_registers_compile,INCLUDE_DIRECTORIES>"
      "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/contract_probe.cpp"
      "-DLOG_DIR=${CMAKE_CURRENT_BINARY_DIR}/contract-results"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/check_contracts.cmake"
    COMMENT "Checking register field collisions and bounds"
    VERBATIM)
endif()
if(GREVIR_BUILD_HOST_TESTS)
  if(NOT TARGET grevir::test_support)
    find_package(grevir-test-support CONFIG REQUIRED)
  endif()
  add_executable(grevir_registers_runtime register_test.cpp selection_test.cpp apply_test.cpp bit_fields_test.cpp extracted_static_tests.cpp mapping_static_tests.cpp)
  target_link_libraries(grevir_registers_runtime PRIVATE grevir::registers grevir::test_support Catch2::Catch2WithMain)
  set_target_properties(grevir_registers_runtime PROPERTIES CXX_EXTENSIONS OFF)
  catch_discover_tests(grevir_registers_runtime TEST_PREFIX "registers."
    PROPERTIES LABELS "registers-mock" TIMEOUT 10)
endif()
