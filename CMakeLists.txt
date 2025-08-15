cmake_minimum_required(VERSION 3.16)

project(BMP LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(BMP
    ${PROJECT_SOURCE_DIR}/OpenGL-Image-Viewer/glad/src/glad.c
    OpenGL-Image-Viewer/glimview.hpp
    OpenGL-Image-Viewer/glimview.cpp
    main.cpp
    bmp.hpp
)

target_include_directories(BMP PRIVATE
    ${OPENGL_INCLUDE_DIRS}
    ${GLFW3_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/OpenGL-Image-Viewer/glad/include
)

target_link_libraries(BMP PRIVATE
    ${OPENGL_LIBRARIES}
    ${GLFW3_LIBRARIES}
    glfw
)

include(GNUInstallDirs)
install(TARGETS BMP
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
