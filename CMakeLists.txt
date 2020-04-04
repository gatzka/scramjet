cmake_minimum_required(VERSION 3.11)
project(scramjet C)

include(FetchContent)
#set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/external/")

FetchContent_Declare(cio
    GIT_REPOSITORY https://github.com/gatzka/cio.git
#GIT_TAG ec44c6c1675c25b9827aacd08c02433cccde7780
)
FetchContent_GetProperties(cio)
if(NOT cio_POPULATED)
    FetchContent_Populate(cio)
    add_subdirectory(${cio_SOURCE_DIR}/src/ ${cio_BINARY_DIR})
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
)
target_link_libraries(scramjet cio::cio)
target_link_libraries(scramjet hs_hash::hs_hash)
target_link_libraries(scramjet sclog::sclog)
set_target_properties(scramjet PROPERTIES
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    C_CLANG_TIDY ""
)


