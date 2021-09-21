cmake_minimum_required(VERSION 3.13)

project (RTMPRestreamer VERSION 0.1)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
option(USE_OPENSSL "Enable OpenSSL (rtmps)" OFF)

if (${USE_OPENSSL})	
	add_definitions(-DUSE_OPENSSL)
endif()

find_package(EasyRTMP REQUIRED)

add_executable(rtmp_restreamer restreamer.cpp)
target_link_libraries(rtmp_restreamer PRIVATE EasyRTMP::easyrtmp)