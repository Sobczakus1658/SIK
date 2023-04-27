cmake_minimum_required(VERSION 3.1)
project(sik-zad1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_FLAGS "-g -Wall -Wextra")

add_executable(sikradio-sender sikradio-sender.c err.h utils.h)
add_executable(sikradio-receiver sikradio-receiver.c err.h utils.h)
target_link_libraries(sikradio-receiver pthread)

install(TARGETS DESTINATION .)