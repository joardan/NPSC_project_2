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

add_library(stb_image STATIC src/stb_image.cpp)
target_include_directories(stb_image PUBLIC include)

add_library(shader_functions INTERFACE)
target_include_directories(shader_functions INTERFACE include)

add_library(glm INTERFACE)
target_include_directories(glm INTERFACE glm)

add_library(camera INTERFACE)
target_include_directories(camera INTERFACE include glm)

# Add the executable main.cpp
#add_executable(OpenGL_Engine src/main.cpp)
# Add the executable main_light.cpp
add_executable(OpenGL_Engine src/main_light.cpp)

# Link libraries
target_link_libraries(OpenGL_Engine PRIVATE glfw OpenGL::GL glad shader_functions stb_image glm camera)