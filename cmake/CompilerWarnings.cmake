function(sh3ds_set_warnings target)
    target_compile_options(
        ${target}
        PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:
                /W4
                /external:W0
                /external:anglebrackets
                /WX
                /permissive-
                /w14242 # conversion, possible loss of data
                /w14254 # operator conversion, possible loss of data
                /w14263 # member function does not override base class virtual
                /w14265 # class has virtual functions but destructor is not virtual
                /w14287 # unsigned/negative constant mismatch
                /w14296 # expression is always false
                /w14311 # pointer truncation
                /w14545 # expression before comma evaluates to function
                /w14546 # function call before comma missing argument list
                /w14547 # operator before comma has no effect
                /w14549 # operator before comma has no effect
                /w14555 # expression has no effect
                /w14619 # pragma warning: nonexistent warning number
                /w14640 # thread-unsafe static member initialization
                /w14826 # conversion is sign-extended
                /w14905 # wide string literal cast to LPSTR
                /w14906 # string literal cast to LPWSTR
                /w14928 # illegal copy-initialization
                /we4289 # loop control variable used outside for-loop
            >
            $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
                -Wall
                -Wextra
                -Wpedantic
                -Werror
                -Wshadow
                -Wnon-virtual-dtor
                -Wold-style-cast
                -Wcast-align
                -Wunused
                -Woverloaded-virtual
                -Wconversion
                -Wsign-conversion
                -Wnull-dereference
                -Wdouble-promotion
                -Wformat=2
                -Wimplicit-fallthrough
            >
            $<$<CXX_COMPILER_ID:GNU>:
                -Wmisleading-indentation
                -Wduplicated-cond
                -Wduplicated-branches
                -Wlogical-op
                -Wuseless-cast
            >
    )
endfunction()
