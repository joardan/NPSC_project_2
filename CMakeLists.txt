cmake_minimum_required(VERSION 3.10)
project(OpenGL_Engine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

set(OpenGL_GL_PREFERENCE GLVND)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Find GLFW (installed earlier)
find_package(glfw3 REQUIRED)

find_package(assimp REQUIRED)

# Add GLAD (manually include the glad.c source file)
add_library(glad STATIC glad/src/glad.c)
target_include_directories(glad PUBLIC glad/include)

add_library(stb_image STATIC src/stb_image.cpp)
target_include_directories(stb_image PUBLIC include)

add_library(imgui
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_tables.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    imgui
    imgui/backends
)

# Add the executable main.cpp
#add_executable(OpenGL_Engine src/main.cpp)
# Add the executable main_light.cpp
add_executable(OpenGL_Engine src/simulation.cpp)

target_include_directories(OpenGL_Engine PRIVATE glm include)
# Link libraries
target_link_libraries(OpenGL_Engine PRIVATE glfw OpenGL::GL imgui glad stb_image assimp)