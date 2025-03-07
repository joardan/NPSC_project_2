cmake_minimum_required(VERSION 3.10)
project(OpenGL_Engine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

set(OpenGL_GL_PREFERENCE GLVND)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Find GLFW (installed earlier)
find_package(glfw3 REQUIRED)

# Add GLAD (manually include the glad.c source file)
add_library(glad STATIC glad/src/glad.c)
target_include_directories(glad PUBLIC glad/include)

add_library(shader INTERFACE)
target_include_directories(shader INTERFACE include)

# Add the executable
add_executable(OpenGL_Engine main.cpp)

# Link libraries
target_link_libraries(OpenGL_Engine PRIVATE glfw OpenGL::GL glad shader)