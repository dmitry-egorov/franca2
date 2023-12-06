cmake_minimum_required(VERSION 3.5)

project(index)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -fno-exceptions -fno-rtti -Wall -Wextra -Werror -Wno-unused -Wno-unknown-pragmas -Wno-missing-field-initializers")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -D_DEBUG=1 -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g0 -DNDEBUG=1 -flto -O3")

file(GLOB headers *.h)
file(GLOB sources *.cpp)

file(COPY ${headers} DESTINATION .)
file(COPY ${sources} DESTINATION .)
file(COPY assets    DESTINATION embedded)
file(COPY main.wgsl DESTINATION embedded)

set(CMAKE_EXECUTABLE_SUFFIX ".html")

# TODO: all the debug flags are link only
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -s DEMANGLE_SUPPORT=1 -s SAFE_HEAP=1 -s STACK_OVERFLOW_CHECK=2")
#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}") #-s DISABLE_EXCEPTION_CATCHING=1")
# --closure 1

set(CMAKE_EXE_LINKER_FLAGS "-s WASM=1 -s USE_WEBGPU=1 -s NO_EXIT_RUNTIME=1 -s STRICT=1 --shell-file ${CMAKE_CURRENT_LIST_DIR}/shell.html --embed-file embedded")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-g3 -D_DEBUG=1 -s ASSERTIONS=1")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-D_NDEBUG=1 -s ASSERTIONS=0")
# -s EVAL_CTORS=1

# Linker flags to optimize for smallest output code_fr size
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s ENVIRONMENT=web -s TEXTDECODER=2 -s ABORTING_MALLOC=0 -s ALLOW_MEMORY_GROWTH=0 -s INITIAL_MEMORY=16MB -s SUPPORT_ERRNO=0 -s MALLOC=emmalloc --output_eol=linux")
#  -s MINIMAL_RUNTIME=2

add_executable(index ${sources} ${platform_sources} ${headers})