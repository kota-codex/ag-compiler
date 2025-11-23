cmake_minimum_required(VERSION 3.15)

project ("ag-compiler")

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  MESSAGE(FATAL_ERROR "Toolchain file not set, try using: cmake --preset default")
endif()

add_subdirectory(ag-ffi-header)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_definitions(-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)

find_package(CURL CONFIG REQUIRED)
find_package(libzip CONFIG REQUIRED)
find_package(LLVM CONFIG REQUIRED)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(llvm_libs ${LLVM_TARGETS_TO_BUILD})

add_executable(agc
    src/utils/utf8.h
    src/utils/utf8.c
    src/ltm/ltm.h
    src/ltm/ltm.cpp

    src/dom/dom.h
    src/dom/dom.cpp

    src/dom/dom-to-string.cpp
    src/dom/dom-to-string.h

    src/ast.h
    src/ast.cpp

    src/parser.h
    src/parser.cpp

    src/name-resolver.h
    src/name-resolver.cpp

    src/type-checker.h
    src/type-checker.cpp

    src/pruner.h
    src/pruner.cpp

    src/const-capture-pass.h
    src/const-capture-pass.cpp

    src/generator.h
    src/generator.cpp

    src/utils/register_runtime.h
    src/utils/register_runtime.cpp

    src/utils/vmt_util.h
    src/compiler.cpp

    src/package-manager.h
    src/package-manager.cpp
)
target_include_directories(agc PRIVATE ${LLVM_INCLUDE_DIRS})
target_include_directories(agc  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(agc PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wno-unqualified-std-cast-call>
)

target_link_libraries(agc PUBLIC ag_ffi_header)

target_link_libraries(agc PRIVATE ${llvm_libs})
target_link_libraries(agc PRIVATE libzip::zip)
target_link_libraries(agc PRIVATE CURL::libcurl)
target_link_libraries(agc PRIVATE ag_ffi_header)

set(DST "$<IF:$<BOOL:${AG_TRIPLE}>,${CMAKE_CURRENT_SOURCE_DIR}/out/bin/${AG_TRIPLE},${CMAKE_CURRENT_SOURCE_DIR}/../out/bin>")
set_target_properties(agc PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${DST}"
)
set(bin_dir "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/bin")
message(STATUS "bin_dir = ${bin_dir}")
file(GLOB dlls CONFIGURE_DEPENDS "${bin_dir}/*.dll" "${bin_dir}/*.so")
message(STATUS "dlls = ${dlls}")
foreach(f ${dlls})
    add_custom_command(TARGET agc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${f} ${DST}
    )
endforeach()
