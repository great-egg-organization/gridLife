cmake_minimum_required (VERSION 4.4 FATAL_ERROR)

project ("gridLife")

set(CMAKE_EXPORT_COMPILE_COMMANDS true)

if (interactive_sim)
	add_compile_definitions(INTERACTIVE_SIM)
endif()

set(CMAKE_CXX_FLAGS "-march=native -mtune=native")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

set (OSBitness 32)
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
  set (OSBitness 64)
endif()
set (FullOutputDir "${CMAKE_SOURCE_DIR}/out/${CMAKE_SYSTEM_NAME}${OSBitness}/${CMAKE_BUILD_TYPE}")
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${FullOutputDir}/static libs")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${FullOutputDir}")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${FullOutputDir}")

if (interactive_sim)
	find_package(SFML REQUIRED COMPONENTS Graphics Window System)
endif()

find_package(GLEW)

# Include sub-projects.
add_subdirectory("gridLife")