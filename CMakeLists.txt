cmake_minimum_required(VERSION 3.11)
project(scramjet C)

include(FetchContent)

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


FetchContent_Declare(hs_hash
    GIT_REPOSITORY https://github.com/gatzka/hs_hash.git
)
FetchContent_GetProperties(hs_hash)
if(NOT hs_hash_POPULATED)
    FetchContent_Populate(hs_hash)
    add_subdirectory(${hs_hash_SOURCE_DIR}/ ${hs_hash_BINARY_DIR})
endif()

FetchContent_Declare(sclog
    GIT_REPOSITORY https://github.com/gatzka/sclog.git
)
FetchContent_GetProperties(sclog)
if(NOT sclog_POPULATED)
    FetchContent_Populate(sclog)
    add_subdirectory(${sclog_SOURCE_DIR}/ ${sclog_BINARY_DIR})
endif()

add_executable(scramjet
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


