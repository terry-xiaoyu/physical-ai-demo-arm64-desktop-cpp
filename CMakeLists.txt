cmake_minimum_required(VERSION 3.1)

project(QuickStart)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(BYTERTC_LINUX 1)
    message("system name: ${CMAKE_SYSTEM_NAME} info:${CMAKE_SYSTEM_PROCESSOR}")
    IF(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86")
        set(SYSTEM_X86 1)
    ELSE(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86")
        set(SYSTEM_X86 0)
    ENDIF(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86")

elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    IF(CMAKE_CL_64)
        set(BYTERTC_WIN32 0)
    ELSE(CMAKE_CL_64)
        set(BYTERTC_WIN32 1)
    ENDIF(CMAKE_CL_64)
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
else()
endif(CMAKE_SYSTEM_NAME MATCHES "Linux")



set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

#set ouput path
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR}/$(Configuration)/QuickStart)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR}/$(Configuration)/QuickStart)
message(STATUS "EXECUTABLE_OUTPUT_PATH = ****${PROJECT_BINARY_DIR}")

set(BYTERTC_SDK_DIR "${CMAKE_SOURCE_DIR}/VolcEngineRTC_arm64")
set(RESOURCES_DIR "${CMAKE_SOURCE_DIR}/resources")
set(CONFIG_FILE "${CMAKE_SOURCE_DIR}/config.json")


find_package(Qt5 COMPONENTS Widgets Core Gui Network REQUIRED)
find_package(OpenSSL REQUIRED)

#link VolcEngineRTC lib
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${BYTERTC_SDK_DIR}/include)
include_directories(${BYTERTC_SDK_DIR}/include/rtc)
include_directories(${BYTERTC_SDK_DIR}/include/game)

set(CMAKE_AUTOMOC TRUE)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

#ui
FILE(GLOB UI_FILES "ui/*.ui")
qt5_wrap_ui(MainWindow_UI_FILES ${UI_FILES})
list(APPEND ALL_SOURCES_AND_HEADERS ${UI_FILES})

message(STATUS "**************${MainWindow_UI_FILES}*************")
qt5_add_resources(MainWindow_QRC_FILES QuickStart.qrc)
#qrc

#sources
FILE(GLOB_RECURSE SOURCES_SOURCES_AND_HEADERS "sources/*.h" "sources/*.cpp")
source_group(sources FILES ${SOURCES_SOURCES_AND_HEADERS})
list(APPEND ALL_SOURCES_AND_HEADERS ${SOURCES_SOURCES_AND_HEADERS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/sources)

add_executable(QuickStart
        ${ALL_SOURCES_AND_HEADERS}
        ${MainWindow_QRC_FILES}
        "app.rc"
        )

IF (BYTERTC_LINUX)
target_link_directories(${PROJECT_NAME} PUBLIC ${BYTERTC_SDK_DIR}/lib/)
set(CMAKE_PREFIX_PATH $ENV{QTDIR}/lib/cmake) #don't forget to set env path QTDIR
target_link_options(${PROJECT_NAME} PUBLIC -Wl,-rpath-link=${BYTERTC_SDK_DIR}/lib/libVolcEngineRTC.so)
set(CMAKE_CXX_FLAGS "-ggdb -std=c++14 -fPIC -pthread")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath='$ORIGIN'")
ENDIF ()

message(STATUS "so=${BYTERTC_SDK_DIR}/lib/libVolcEngineRTC.so")

find_path(PULSEAUDIO_INCLUDE_DIR
        NAMES pulse/pulseaudio.h
        DOC "The PulseAudio include directory"
    )
find_library(PULSEAUDIO_LIBRARY
        NAMES pulse
        DOC "The PulseAudio library"
    )
target_include_directories(${PROJECT_NAME} PUBLIC ${PULSEAUDIO_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PUBLIC
        Qt5::Widgets
        Qt5::Network
        # RTCFFmpeg
        VolcEngineRTC
        pulse-simple pulse
        atomic
        OpenSSL::Crypto
        )

set(DST_DIR "${PROJECT_BINARY_DIR}")
set(LIB_DIR "${BYTERTC_SDK_DIR}/lib")
set(ARCHIVE_DIR archive)


add_custom_target(
  archive
  COMMAND ${CMAKE_COMMAND} -E make_directory ${ARCHIVE_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy  ${LIB_DIR}/*.so ${ARCHIVE_DIR}/
  COMMAND ${CMAKE_COMMAND} -E copy  ${CMAKE_BINARY_DIR}/${PROJECT_NAME}  ${ARCHIVE_DIR}/
  COMMAND patchelf --set-rpath '$$ORIGIN' ${CMAKE_BINARY_DIR}/${ARCHIVE_DIR}/${PROJECT_NAME}
  COMMAND ${CMAKE_COMMAND} -E copy  ${CMAKE_SOURCE_DIR}/default.desktop  ${ARCHIVE_DIR}/default.desktop
  COMMAND linuxdeployqt ${CMAKE_BINARY_DIR}/${ARCHIVE_DIR}/${PROJECT_NAME} -always-overwrite -appimage
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_dependencies(archive ${PROJECT_NAME})

add_custom_command(
  TARGET archive POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E tar cvf "${ARCHIVE_DIR}.zip" --format=zip
  ${ARCHIVE_DIR}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
