cmake_minimum_required(VERSION 3.26)

project(ProPinballSuperUltra)

find_package(Freetype REQUIRED)

add_library(hook SHARED main.c)
target_link_libraries(hook Freetype::Freetype)
