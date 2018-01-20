cmake_minimum_required (VERSION 2.6)
project (gst-transformer)

find_library(FMT_LIB fmt)
if(NOT FMT_LIB)
  message(FATAL_ERROR "fmt library not found: https://github.com/fmtlib/fmt")
endif()

find_package(PkgConfig)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-app-1.0)

set(CMAKE_BUILD_TYPE "Debug")

set(Compiler_Flags_Common_Debug "-std=c++11 -O0 -g -fPIC -Wall -frtti")
set(Compiler_Flags_Common_Release "-std=c++11 -O3 -DNDEBUG -fPIC -Wall -frtti")

set(CMAKE_COMPILER_IS_GNUCC TRUE)

set(CMAKE_C_FLAGS_DEBUG "${Compiler_Flags_C} ${Compiler_Flags_Common_Debug} ${Compile_Options_Debug}")
set(CMAKE_C_FLAGS_RELEASE "${Compiler_Flags_C} ${Compiler_Flags_Common_Release} ${Compile_Options_Release}")

set(CMAKE_CXX_FLAGS_DEBUG "${Compiler_Flags_CPP} ${Compiler_Flags_Common_Debug} ${Compile_Options_Debug}")
set(CMAKE_CXX_FLAGS_RELEASE "${Compiler_Flags_CPP} ${Compiler_Flags_Common_Release} ${Compile_Options_Release}")

include_directories(${GST_INCLUDE_DIRS})
include_directories(src/lib)
include_directories(${CMAKE_BINARY_DIR})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
find_package(Protobuf REQUIRED)
find_package(GRPC REQUIRED)
file(GLOB ProtoFiles src/lib/server/protos/*.proto)
PROTOBUF_GENERATE_CPP(ProtoSources ProtoHeaders ${ProtoFiles})
GRPC_GENERATE_CPP(GrpcSources ProtoHeaders ${CMAKE_BINARY_DIR} ${ProtoFiles})
add_library(proto STATIC ${GrpcSources} ${ProtoSources})

file(GLOB LibSources src/lib/*.cpp)
add_library(gsttransformer SHARED ${LibSources})
target_link_libraries(gsttransformer fmt pthread ${GST_LIBRARIES})

file(GLOB ServerLibSources src/lib/server/*.cpp)
add_library(gsttransformer_server SHARED ${ServerLibSources})
target_link_libraries(gsttransformer_server gsttransformer)
add_dependencies(gsttransformer_server proto)

file(GLOB ServerSources src/server/*.cpp)
add_executable(gsttransformerserver ${ServerSources})
target_link_libraries(gsttransformerserver gsttransformer_server gsttransformer proto fmt pthread ${PROTOBUF_LIBRARIES} gRPC::grpc++)
target_include_directories(gsttransformerserver PUBLIC src/lib/server)

set(TestClient clientsamples/cpp/client.cpp clientsamples/cpp/clientcli.cpp)
add_executable(gsttransformerclient clientsamples/cpp/gst-transformer-client.cpp ${TestClient})
target_link_libraries(gsttransformerclient proto fmt pthread proto ${PROTOBUF_LIBRARIES} gRPC::grpc++ uuid)

add_executable(gsttransformerinproc clientsamples/cpp/gst-transformer-inproc.cpp ${TestClient})
target_link_libraries(gsttransformerinproc gsttransformer_server gsttransformer proto fmt pthread proto ${PROTOBUF_LIBRARIES} gRPC::grpc++ uuid)
target_include_directories(gsttransformerinproc PUBLIC src/lib/server)
