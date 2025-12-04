cmake_minimum_required(VERSION 3.1)
project(video_pipe)
set(CMAKE_CXX_STANDARD 14)

find_package(PkgConfig REQUIRED)

# GStreamer
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-audio-1.0 gstreamer-video-1.0  gstreamer-app-1.0)
message("GST_INCLUDE_DIRS = ${GST_INCLUDE_DIRS}")
message("GST_LIBRARY_DIRS = ${GST_LIBRARY_DIRS}")
message("GST_LIBRARIES = ${GST_LIBRARIES}")

link_directories( ${GST_LIBRARY_DIRS}  )
include_directories( ${GST_INCLUDE_DIRS}  )


# OpenCV ${OpenCV_LIBS}, used for some examples
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV Include dirs = ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV LIBRARIES = ${OpenCV_LIBS}")

add_executable(${PROJECT_NAME} src/main.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${OpenCV_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${GST_LIBRARIES} ${OpenCV_LIBS})

