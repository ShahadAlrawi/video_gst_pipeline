#!/bin/bash
echo "[Note]: Load vision_ai"
sudo xmutil unloadapp
sudo xmutil loadapp vision_ai

# sleep for 10 second
sleep 10s

# give all rw access to dpu - needed to run dpu from petalinux
echo "[Note]: Give petalinux user r/w access to DPU"
sudo chmod 666 /dev/dpu

# setup cameras
echo "[Note]: Setup v4l2 camera device subsystems"
media-ctl -v -V '"ap1302.4-003c":2 [fmt:UYVY8_1X16/1920x1080 field:none colorspace:srgb]' &> /dev/null
media-ctl -v -V '"b0040000.v_proc_ss":0 [fmt:UYVY8_1X16/1920x1080 field:none colorspace:srgb]' &> /dev/null
media-ctl -v -V '"b0040000.v_proc_ss":1 [fmt:RBG888_1X24/1920x1080 field:none colorspace:srgb]' &> /dev/null

# setup video format
echo "[Note]: Setup video format and test stream"
v4l2-ctl -d /dev/video0  --set-fmt-video=width=1920,height=1080,pixelformat=RGB3 --stream-mmap --stream-count=1  &> /dev/null


if [[ $? -eq 0 ]]; then
  echo "[Note]: Successfully loaded FPGA bitstream and set camera output"
else
  echo "[Error]: Failed to load FPGA bitstream and set camera output"
fi