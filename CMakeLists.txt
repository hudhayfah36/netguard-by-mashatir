# ============================================================================
#  NetGuard by Mashatir - ESP-IDF project root CMakeLists.txt
#  Build with:  idf.py set-target esp32s3 && idf.py menuconfig && idf.py build flash monitor
# ============================================================================
cmake_minimum_required(VERSION 3.16)
set(PROJECT_VER "0.1.0")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(netguard)
