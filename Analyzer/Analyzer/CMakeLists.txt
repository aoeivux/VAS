cmake_minimum_required(VERSION 3.16.0)
project(Analyzer_v2 VERSION 2.0)

#set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread ")


set(CMAKE_BUILD_TYPE DEBUG)
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")


# set(INCLUDE_DIR /usr/local/include)
# set(LIB_DIR /usr/local/lib)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/event/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/curl/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/jsoncpp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/ffmpeg/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/jpeg-turbo/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/opencv/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../path/to/AdditionalIncludeDirectories
)

link_directories(../3rdparty/ffmpeg/x64/lib/)


# find_package(OpenCV REQUIRED)

set(source
        Core/Analyzer.cpp
        Core/AvPullStream.cpp
        Core/AvPushStream.cpp
        Core/Config.cpp
        Core/ControlExecutor.cpp
        Core/GenerateAlarm.cpp
        Core/GenerateVideo.cpp
        Core/Scheduler.cpp
        Core/Server.cpp
        Core/Utils/Request.cpp
        main.cpp
        )

add_executable(Analyzer_v2 ${source})
# target_link_libraries(Analyzer_v2 ${OpenCV_LIBS})
# target_link_libraries(Analyzer_v2 event curl jsoncpp avformat avcodec avutil swscale swresample )

target_link_libraries(Analyzer_v2
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/event/x64/lib/Release/*.lib
    ws2_32.lib
    iphlpapi.lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/jpeg-turbo/x64/lib/Release/turbojpeg.lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/curl/x64/lib/Release/libcurl.lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/jsoncpp/x64/lib/Release/jsoncpp.lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/opencv/x64/vc15/lib/opencv_world3410.lib
    avcodec.lib
    avformat.lib
    avutil.lib
    avdevice.lib
    avfilter.lib
    postproc.lib
    swresample.lib
    swscale.lib
)
