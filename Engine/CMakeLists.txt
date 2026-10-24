cmake_minimum_required(VERSION 3.20)

# Include header directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Engine Core Source Files
set(ENGINE_SOURCES
    src/Image.cpp
    src/BMPReader.cpp
    src/BMPWriter.cpp
    src/BMPRLE8Writer.cpp
    src/BitWriter.cpp
    src/BitReader.cpp
    src/RLEEngine.cpp
    src/HuffmanTree.cpp
    src/LosslessEngine.cpp
    src/DCT.cpp
    src/Metrics.cpp
    src/LossyEngine.cpp
    src/HttpServer.cpp
)

# Core Static Library
add_library(pixelforge_engine STATIC ${ENGINE_SOURCES})
target_include_directories(pixelforge_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Link Windows Socket Library (ws2_32) for native networking
if (WIN32)
    target_link_libraries(pixelforge_engine PUBLIC ws2_32)
endif()

# Main PixelForge CLI & Web Server Executable
add_executable(pixelforge src/main.cpp)
target_link_libraries(pixelforge PRIVATE pixelforge_engine)

# Unit Tests & Utilities
add_executable(test_bmp tests/test_bmp.cpp)
target_link_libraries(test_bmp PRIVATE pixelforge_engine)
add_test(NAME BMPRoundTripTest COMMAND test_bmp)
set_tests_properties(BMPRoundTripTest PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(test_lossless tests/test_lossless.cpp)
target_link_libraries(test_lossless PRIVATE pixelforge_engine)
add_test(NAME LosslessRoundTripTest COMMAND test_lossless)
set_tests_properties(LosslessRoundTripTest PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(test_lossy tests/test_lossy.cpp)
target_link_libraries(test_lossy PRIVATE pixelforge_engine)
add_test(NAME LossyRoundTripTest COMMAND test_lossy)
set_tests_properties(LossyRoundTripTest PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(generate_samples tests/generate_samples.cpp)
target_link_libraries(generate_samples PRIVATE pixelforge_engine)




