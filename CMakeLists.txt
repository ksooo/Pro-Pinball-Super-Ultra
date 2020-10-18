project(ProPinballSuperUltra)

find_package(Freetype REQUIRED)

add_library(hook SHARED main.c)
target_link_libraries(hook Freetype::Freetype)
