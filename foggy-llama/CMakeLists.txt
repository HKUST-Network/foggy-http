cmake_minimum_required(VERSION 3.16)
project(foggy_llama_cp4 LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Threads REQUIRED)

option(FOGGY_WITH_LLAMA "Enable the real llama.cpp inference engine" OFF)
set(LLAMA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/llama.cpp" CACHE PATH "Pinned llama.cpp checkout")
set(FOGGY_HTTP_READ_MS 120000 CACHE STRING "Response header / chunk read timeout (milliseconds)")
if(NOT FOGGY_HTTP_READ_MS MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "FOGGY_HTTP_READ_MS must be positive")
endif()

set(CP3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../foggy-http")
set(FOGGY_TCP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/foggytcp/foggytcp")
if(NOT EXISTS "${FOGGY_TCP_DIR}/inc/foggy_tcp.h")
    message(FATAL_ERROR "Initialize thirdparty/foggytcp from the repository root with git submodule update --init")
endif()

add_library(foggy_transport STATIC
    "${CP3_DIR}/src/transport.cc"
    "${FOGGY_TCP_DIR}/src/foggy_tcp.cc"
    "${FOGGY_TCP_DIR}/src/foggy_backend.cc"
    "${FOGGY_TCP_DIR}/src/foggy_function.cc"
    "${FOGGY_TCP_DIR}/src/foggy_packet.cc")
target_include_directories(foggy_transport PUBLIC "${CP3_DIR}/inc" PRIVATE "${FOGGY_TCP_DIR}/inc")
target_compile_options(foggy_transport PRIVATE -include "${CP3_DIR}/inc/transport_log.h")
target_link_libraries(foggy_transport PUBLIC Threads::Threads)

add_library(foggy_http STATIC src/foggy_http.c)
target_include_directories(foggy_http PUBLIC src)
target_compile_definitions(foggy_http PRIVATE _POSIX_C_SOURCE=200809L HTTP_READ_MS=${FOGGY_HTTP_READ_MS})
target_compile_options(foggy_http PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(foggy_http PUBLIC foggy_transport)
add_library(foggy_api STATIC src/api.cpp src/mock_engine.cpp src/client_flow.cpp
    src/router.cpp src/llama_routes.cpp
    src/rate_limiter.cpp)
target_include_directories(foggy_api PUBLIC src vendor)
target_compile_options(foggy_api PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(foggy_api PUBLIC foggy_http)
add_executable(foggy-server src/server.cpp)
target_link_libraries(foggy-server PRIVATE foggy_api)
add_executable(foggy-client src/client.cpp)
target_link_libraries(foggy-client PRIVATE foggy_api)

if(FOGGY_WITH_LLAMA)
    if(NOT EXISTS "${LLAMA_SOURCE_DIR}/include/llama.h")
        message(FATAL_ERROR "Initialize thirdparty/llama.cpp from the repository root or set LLAMA_SOURCE_DIR")
    endif()
    find_package(Git REQUIRED)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/llama-revision.txt" llama_revision LIMIT_COUNT 1)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${LLAMA_SOURCE_DIR}" rev-parse HEAD
        OUTPUT_VARIABLE llama_head OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE llama_git_status ERROR_QUIET)
    if(NOT llama_git_status EQUAL 0 OR NOT llama_head STREQUAL llama_revision)
        message(FATAL_ERROR "LLAMA_SOURCE_DIR must be a Git checkout at ${llama_revision}")
    endif()
    # Only link inference libraries; keep HTTP and transport in CP3.
    foreach(flag LLAMA_BUILD_COMMON LLAMA_BUILD_TOOLS LLAMA_BUILD_EXAMPLES
                 LLAMA_BUILD_TESTS LLAMA_BUILD_SERVER LLAMA_BUILD_APP LLAMA_BUILD_UI
                 GGML_METAL GGML_CUDA GGML_NATIVE GGML_OPENMP BUILD_SHARED_LIBS)
        set(${flag} OFF CACHE BOOL "" FORCE)
    endforeach()
    add_subdirectory("${LLAMA_SOURCE_DIR}" llama-build EXCLUDE_FROM_ALL)
    target_sources(foggy-server PRIVATE src/llama_engine.cpp)
    target_compile_definitions(foggy-server PRIVATE FOGGY_WITH_LLAMA)
    target_link_libraries(foggy-server PRIVATE llama)
endif()
