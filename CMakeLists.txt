cmake_minimum_required(VERSION 3.14)

include(FetchContent)

FetchContent_Declare(GatzkaToolchainFiles
    GIT_REPOSITORY https://github.com/gatzka/cmake-toolchain-files.git
    SOURCE_DIR ${CMAKE_BINARY_DIR}/toolchains
)
FetchContent_MakeAvailable(GatzkaToolchainFiles)

project(scramjet C)

include(CTest)

find_package(cio QUIET)
if(cio_FOUND)
    message(STATUS "cio already installed, using it")
else()
    message(STATUS "cio not installed, fetching it")
    FetchContent_Declare(cio
        GIT_REPOSITORY https://github.com/gatzka/cio.git
    )
    FetchContent_GetProperties(cio)
    if(NOT cio_POPULATED)
        FetchContent_Populate(cio)
        add_subdirectory(${cio_SOURCE_DIR}/lib/ ${cio_BINARY_DIR})
    endif()
endif()

find_package(hs_hash QUIET)
if(hs_hash_FOUND)
    message(STATUS "hs_hash already installed, using it")
else()
    message(STATUS "hs_hash not installed, fetching it")
    FetchContent_Declare(hs_hash
        GIT_REPOSITORY https://github.com/gatzka/hs_hash.git
    )
    FetchContent_MakeAvailable(hs_hash)
endif()

find_package(sclog QUIET)
if(sclog_FOUND)
    message(STATUS "sclog already installed, using it")
else()
    message(STATUS "sclog not installed, fetching it")
    FetchContent_Declare(sclog
        GIT_REPOSITORY https://github.com/gatzka/sclog.git
    )
    FetchContent_GetProperties(sclog)
    if(NOT sclog_POPULATED)
        FetchContent_Populate(sclog)
        add_subdirectory(${sclog_SOURCE_DIR}/lib/ ${sclog_BINARY_DIR})
    endif()
endif()

add_executable(scramjet
    api_version.c
    api_version.h
    jet_client.h
    messages.c
    messages.h
    scramjet.c
    sj_log.c
)
target_link_libraries(scramjet cio::cio)
target_link_libraries(scramjet hs_hash::hs_hash)
target_link_libraries(scramjet sclog::sclog)
set_target_properties(scramjet PROPERTIES
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    C_CLANG_TIDY ""
)
install(TARGETS scramjet RUNTIME DESTINATION bin)
