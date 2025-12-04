#
# Copyright 2022-2023 Advanced Micro Devices Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
target=video_gst
result=0 && pkg-config --list-all | grep opencv4 && result=1
if [ $result -eq 1 ]; then
	OPENCV_FLAGS=$(pkg-config --cflags --libs-only-L opencv4)
else
	OPENCV_FLAGS=$(pkg-config --cflags --libs-only-L opencv)
fi

GST_FLAGS=$(pkg-config --cflags --libs gstreamer-1.0)
CXX=${CXX:-g++}

for file in $(ls src/*.cpp); do
	filelist="$filelist ${file}"
done

echo $filelist

$CXX -o ${target} ${filelist} -std=c++17 -O3 ${GST_FLAGS} ${OPENCV_FLAGS} \
-lglog -lxir -lopencv_imgproc -lopencv_core -lvart-runner -lvitis_ai_library-yolov6
