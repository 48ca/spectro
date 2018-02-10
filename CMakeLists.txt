cmake_minimum_required(VERSION 2.8)
project(spectro)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")

set (CMAKE_CXX_STANDARD 14)

find_package(SDL2)
list(APPEND INCLUDE ${SDL2_INCLUDE_DIRS})
list(APPEND LIBRARIES ${SDL2_LIBRARIES})

find_package(SDL2_mixer)
list(APPEND INCLUDE ${SDL2_MIXER_INCLUDE_DIRS})
list(APPEND LIBRARIES ${SDL2_MIXER_LIBRARIES})

find_package(LibSndFile ${PACKAGE_OPTION})
list(APPEND INCLUDE ${LIBSNDFILE_INCLUDE_DIRS})
list(APPEND LIBRARIES ${LIBSNDFILE_LIBRARIES})

if(MSVC)
	# Visual Studio -- /W4
	if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
	endif()
elseif(CMAKE_COMPILER_IS_GNUXX OR CMAKE_COMPILER_IS_GNUCC)
	# GCC -- -Wall -pedantic
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wno-implicit-fallthrough")
endif()

add_executable(spectro spectro.cpp)
target_include_directories( spectro PRIVATE ${INCLUDE})
target_link_libraries(spectro ${LIBRARIES})

install(TARGETS spectro DESTINATION bin)
