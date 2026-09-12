cmake_minimum_required(VERSION 3.16)
project(mouse_light LANGUAGES CXX)

add_executable(mouse_light WIN32 MouseRipple.cpp)
target_compile_features(mouse_light PRIVATE cxx_std_17)
target_compile_definitions(mouse_light PRIVATE UNICODE _UNICODE)
target_link_libraries(mouse_light PRIVATE gdiplus user32 gdi32 shell32 comctl32 comdlg32 advapi32)
