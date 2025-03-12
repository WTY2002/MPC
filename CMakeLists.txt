cmake_minimum_required(VERSION 3.10)
project(MPCP)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(cppzmq REQUIRED)

if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-w)
endif()

add_compile_definitions(
        SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_library(MPC
        src/Util.cc
        src/PCNode.cc
        src/NetworkNode.cc
        src/JMPProtocol.cc
        src/AddProtocol.cc
        src/MulProtocol.cc
        src/RecProtocol.cc
        src/SharingProtocol.cc
        src/ReSharingProtocol.cc
        src/DotProductProtocol.cc
        src/TruncationProtocol.cc
        src/A2BProtocol.cc
        src/ProcessTrun.cc
        src/ProcessA2B.cc
)

target_link_libraries(MPC
        PUBLIC
        cppzmq
        pthread
        OpenSSL::Crypto
)

macro(add_protocol_executable TARGET_NAME SOURCE_FILE)
    add_executable(${TARGET_NAME} ${SOURCE_FILE})
    target_link_libraries(${TARGET_NAME}
            PRIVATE
            MPC
            ${CMAKE_THREAD_LIBS_INIT}
            cppzmq
            OpenSSL::Crypto
    )
endmacro()

add_protocol_executable(ShareNode tests/ShareNode.cc)
add_protocol_executable(RecNode tests/RecNode.cc)
add_protocol_executable(ReShareNode tests/ReShareNode.cc)
add_protocol_executable(MulNode tests/MulNode.cc)
add_protocol_executable(DotProductNode tests/DotProductNode.cc)
add_protocol_executable(TrunNode tests/TrunNode.cc)
add_protocol_executable(A2BNode tests/A2BNode.cc)

add_protocol_executable(FCNN benchmark/FCNN.cc)
add_protocol_executable(FCNN_MPC benchmark/FCNN_MPC.cc)
