cmake_minimum_required(VERSION 3.8)
project(httpclient)

add_definitions(-DPLATFORM_LINUX)

include_directories(
		./http/inc/
		./cjson/inc/
)

set(CMAKE_BUILD_TYPE "Release")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

set(SOURCE_FILES
        ./http/src/http.c
        ./http/src/http_parser.c
		./cjson/src/cJSON.c
		./cjson/src/cJSON_Utils.c
        main.c)

add_executable(httpclient ${SOURCE_FILES})