find_package(GTest REQUIRED)
include(GoogleTest)

function(sh3ds_add_test test_name test_source)
  add_executable(${test_name} ${test_source})
  target_link_libraries(
    ${test_name}
    PRIVATE
      GTest::gtest
      GTest::gtest_main
  )
  sh3ds_set_warnings(${test_name})
  gtest_discover_tests(${test_name})
endfunction()

# Unit tests
sh3ds_add_test(TestTypes unit/TestTypes.cpp)
target_link_libraries(TestTypes PRIVATE SH3DS::Core SH3DS::Input)

sh3ds_add_test(TestConfig unit/TestConfig.cpp)
target_link_libraries(TestConfig PRIVATE SH3DS::Core)

sh3ds_add_test(TestInputEncoding unit/TestInputEncoding.cpp)
target_link_libraries(TestInputEncoding PRIVATE SH3DS::Input)

sh3ds_add_test(TestFramePreprocessor unit/TestFramePreprocessor.cpp)
target_link_libraries(TestFramePreprocessor PRIVATE SH3DS::Capture)

sh3ds_add_test(TestFsmTransitions unit/TestFsmTransitions.cpp)
target_link_libraries(TestFsmTransitions PRIVATE SH3DS::FSM)

sh3ds_add_test(TestShinyDetector unit/TestShinyDetector.cpp)
target_link_libraries(TestShinyDetector PRIVATE SH3DS::Vision)

# TODO: enable remaining tests once their deps compile
#
# sh3ds_add_test(TestReplayPipeline integration/TestReplayPipeline.cpp)
# target_link_libraries(TestReplayPipeline PRIVATE SH3DS::Orchestrator)
