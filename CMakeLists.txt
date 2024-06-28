cmake_minimum_required(VERSION 3.0)
project(elden_tracker)


find_package(asio CONFIG REQUIRED)

include_directories(src)
add_library(eldentracker SHARED src/main.cpp src/mem.cpp src/settings.cpp src/functionality.cpp src/networking.cpp src/logger.cpp)
target_link_libraries(eldentracker PRIVATE asio::asio)

