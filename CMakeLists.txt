cmake_minimum_required(VERSION 2.8)
project(spectro)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")

set (CMAKE_CXX_STANDARD 14)

find_package(SDL_mixer)
find_package(LibSndFile ${PACKAGE_OPTION})

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
target_include_directories( spectro PRIVATE ${SDL_MIXER_INCLUDE_DIRS} ${LIBSNDFILE_INCLUDE_DIRS})
target_link_libraries(spectro ${SDL_MIXER_LIBRARIES} ${LIBSNDFILE_LIBRARIES})

install(TARGETS spectro DESTINATION bin)
