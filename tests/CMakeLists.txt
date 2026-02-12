find_package(GTest REQUIRED)
include(GoogleTest)

function(sh3ds_add_test test_name test_source)
  add_executable(${test_name} ${test_source})
  target_link_libraries(
    ${test_name}
    PRIVATE
      SH3DS::Orchestrator
      GTest::gtest
      GTest::gtest_main
  )
  sh3ds_set_warnings(${test_name})
  gtest_discover_tests(${test_name})
endfunction()

# Unit tests
sh3ds_add_test(TestTypes unit/TestTypes.cpp)
sh3ds_add_test(TestConfig unit/TestConfig.cpp)
sh3ds_add_test(TestInputEncoding unit/TestInputEncoding.cpp)
sh3ds_add_test(TestFramePreprocessor unit/TestFramePreprocessor.cpp)
