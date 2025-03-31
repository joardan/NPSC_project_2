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

# Add the executable main.cpp
#add_executable(OpenGL_Engine src/main.cpp)
# Add the executable main_light.cpp
add_executable(OpenGL_Engine src/cubemap_r.cpp)

target_include_directories(OpenGL_Engine PRIVATE glm include)
# Link libraries
target_link_libraries(OpenGL_Engine PRIVATE glfw OpenGL::GL glad stb_image assimp)