cmake_minimum_required(VERSION 3.11)
project(scramjet C)

include(FetchContent)
FetchContent_Declare(cio
    GIT_REPOSITORY https://github.com/gatzka/cio.git
#GIT_TAG ec44c6c1675c25b9827aacd08c02433cccde7780
)
FetchContent_GetProperties(cio)
if(NOT cio_POPULATED)
    FetchContent_Populate(cio)
    add_subdirectory(${cio_SOURCE_DIR}/src/ ${cio_BINARY_DIR})
endif()

add_executable(scramjet
    scramjet.c
)
target_link_libraries(scramjet)
target_link_libraries(scramjet cio:cio)
set_target_properties(scramjet PROPERTIES
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    C_CLANG_TIDY ""
)


