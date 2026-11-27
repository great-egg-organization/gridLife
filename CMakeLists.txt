cmake_minimum_required (VERSION 4.4 FATAL_ERROR)

project ("gridLife")

set(CMAKE_EXPORT_COMPILE_COMMANDS true)

set(CACHE{interactive_sim} TYPE BOOL VALUE false)

if (interactive_sim)
	message("Building with gui!")
	add_compile_definitions(INTERACTIVE_SIM)
else()
	message("Building without gui!")
endif()

set(CMAKE_CXX_FLAGS "-march=native -mtune=native -fno-omit-frame-pointer")
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
	set(SFML_VERSION "3.1.0")
	
	include(FetchContent)

	FetchContent_Declare(
    sfml
    GIT_REPOSITORY "https://github.com/SFML/SFML.git"
    GIT_TAG        "${SFML_VERSION}"
	)

	FetchContent_GetProperties(sfml)
	if(NOT sfml_POPULATED)
    FetchContent_MakeAvailable(sfml)
	endif()
endif()

# Include sub-projects.
add_subdirectory("gridLife")

unset(interactive_sim CACHE)
