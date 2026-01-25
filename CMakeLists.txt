cmake_minimum_required(VERSION 3.1)
project(video_pipe)

find_package(PkgConfig REQUIRED)

# GStreamer
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-audio-1.0 gstreamer-video-1.0  gstreamer-app-1.0)
message("GST_INCLUDE_DIRS = ${GST_INCLUDE_DIRS}")
message("GST_LIBRARY_DIRS = ${GST_LIBRARY_DIRS}")
message("GST_LIBRARIES = ${GST_LIBRARIES}")

# OpenCV ${OpenCV_LIBS}, used for some examples
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV Include dirs = ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV LIBRARIES = ${OpenCV_LIBS}")


add_executable(${PROJECT_NAME} src/main.cpp)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

target_include_directories(${PROJECT_NAME} PUBLIC ${OpenCV_INCLUDE_DIRS} PUBLIC ${GST_INCLUDE_DIRS}  PUBLIC ./include)

target_link_libraries(${PROJECT_NAME} 
    ${OpenCV_LIBS} ${GST_LIBRARY_DIRS} ${GST_LIBRARIES}
    vitis_ai_library-yolov6 vitis_ai_library-dpu_task vitis_ai_library-xnnpp 
    vitis_ai_library-model_config vitis_ai_library-math vart-util xir)

