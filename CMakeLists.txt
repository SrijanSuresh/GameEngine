cmake_minimum_required(VERSION 3.20)
project(NovaEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies ─────────────────────────────────────────────────────────────
include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# GLAD  (OpenGL 4.6 Core)
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v0.1.36
)
FetchContent_MakeAvailable(glad)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

# ── Sources ───────────────────────────────────────────────────────────────────
add_executable(NovaEngine
    src/main.cpp
    src/Core/Application.cpp
)

target_include_directories(NovaEngine PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${glad_SOURCE_DIR}/include
)

target_link_libraries(NovaEngine PRIVATE
    glfw
    glad
    glm::glm
    GL          # Linux OpenGL
    dl          # Required by GLAD on Linux
)

# ── Compiler flags ────────────────────────────────────────────────────────────
target_compile_options(NovaEngine PRIVATE
    -Wall
    -Wextra
    -O2
)