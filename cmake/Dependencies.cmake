# Git submodule initialization
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Initializing git submodules...")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            RESULT_VARIABLE GIT_SUBMOD_RESULT
        )
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}")
        endif()
    endif()
endif()

# Verify kappa-core submodule exists
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/external/kappa-core/CMakeLists.txt")
    message(FATAL_ERROR "kappa-core submodule not found. Run: git submodule update --init --recursive")
else()
    add_subdirectory(external/kappa-core)
endif()
