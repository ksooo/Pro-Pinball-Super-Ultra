project(ProPinballSuperUltra)

find_package(Freetype REQUIRED)
find_package(SDL2 REQUIRED)

add_library(hook SHARED main.c)
target_link_libraries(hook Freetype::Freetype SDL2::SDL2)
