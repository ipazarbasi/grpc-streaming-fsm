cmake_minimum_required(VERSION 3.14)
project(
  grpc-long-running-task
  VERSION 1.0.0
  LANGUAGES CXX)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${PROJECT_SOURCE_DIR}/cmake/module")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(gRPC REQUIRED)

include(GenerateGRPC)

run_grpc_generators(${PROJECT_SOURCE_DIR}/proto ${PROJECT_SOURCE_DIR}/generated)

# FIXME: Obtain generated file list from the generator
add_executable(lrr-client "client.cpp" "state_machine_types.h" "${PROJECT_SOURCE_DIR}/generated/longrunningservice.grpc.pb.cc" "${PROJECT_SOURCE_DIR}/generated/longrunningservice.pb.cc")
target_include_directories(lrr-client PRIVATE "${PROJECT_SOURCE_DIR}/generated")
target_link_libraries(lrr-client gRPC::grpc++)

add_executable(lrr-server "server.cpp" "state_machine_types.h" "${PROJECT_SOURCE_DIR}/generated/longrunningservice.grpc.pb.cc" "${PROJECT_SOURCE_DIR}/generated/longrunningservice.pb.cc")
target_include_directories(lrr-server PRIVATE "${PROJECT_SOURCE_DIR}/generated")
target_link_libraries(lrr-server gRPC::grpc++)