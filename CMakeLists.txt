cmake_minimum_required(VERSION 3.10)
project(SmartImageCompressor LANGUAGES CXX)

# C++17 required for std::filesystem
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find OpenCV (Windows: cmake -DOpenCV_DIR=C:/opencv/build ...)
find_package(OpenCV REQUIRED)

# Core class implementations
set(CORE_SOURCES
    src/LosslessCompressor.cpp
)

add_executable(SmartImageCompressor
    ${CORE_SOURCES}
    app/main.cpp
)

target_include_directories(SmartImageCompressor PRIVATE
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(SmartImageCompressor PRIVATE ${OpenCV_LIBS})

if(MSVC)
    target_compile_options(SmartImageCompressor PRIVATE /W3 /utf-8)
else()
    target_compile_options(SmartImageCompressor PRIVATE -Wall -Wextra)
endif()
