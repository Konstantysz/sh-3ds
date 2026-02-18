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

sh3ds_add_test(TestHuntProfiles unit/TestHuntProfiles.cpp)
target_link_libraries(TestHuntProfiles PRIVATE SH3DS::FSM)

sh3ds_add_test(TestSoftResetStrategy unit/TestSoftResetStrategy.cpp)
target_link_libraries(TestSoftResetStrategy PRIVATE SH3DS::Strategy)

sh3ds_add_test(TestShinyDetector unit/TestShinyDetector.cpp)
target_link_libraries(TestShinyDetector PRIVATE SH3DS::Vision)

sh3ds_add_test(TestFileFrameSourceSeek unit/TestFileFrameSourceSeek.cpp)
target_link_libraries(TestFileFrameSourceSeek PRIVATE SH3DS::Capture)

sh3ds_add_test(TestFramePreprocessorDualScreen unit/TestFramePreprocessorDualScreen.cpp)
target_link_libraries(TestFramePreprocessorDualScreen PRIVATE SH3DS::Capture)

sh3ds_add_test(TestConfigBottomScreen unit/TestConfigBottomScreen.cpp)
target_link_libraries(TestConfigBottomScreen PRIVATE SH3DS::Core)

sh3ds_add_test(TestVideoFrameSource unit/TestVideoFrameSource.cpp)
target_link_libraries(TestVideoFrameSource PRIVATE SH3DS::Capture)

sh3ds_add_test(TestScreenDetector unit/TestScreenDetector.cpp)
target_link_libraries(TestScreenDetector PRIVATE SH3DS::Capture)

# Integration tests
sh3ds_add_test(TestReplayPipeline integration/TestReplayPipeline.cpp)
target_link_libraries(TestReplayPipeline PRIVATE SH3DS::Core SH3DS::Capture SH3DS::FSM SH3DS::Vision)
