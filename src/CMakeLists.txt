find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs videoio)
find_package(yaml-cpp REQUIRED)

add_subdirectory(Core)
add_subdirectory(Input)
add_subdirectory(Capture)
add_subdirectory(FSM)

add_subdirectory(Vision)
add_subdirectory(Strategy)
add_subdirectory(Pipeline)
add_subdirectory(App)

add_subdirectory(Sh3DSApp)
