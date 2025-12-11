cmake_minimum_required(VERSION 3.16)
project(CS101AFinalProj CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(sfml 
    GIT_REPOSITORY https://github.com/SFML/SFML.git 
    GIT_TAG 3.0.2
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(sfml)

add_executable(CS101AFinalProj test.cpp)
target_compile_features(CS101AFinalProj PRIVATE cxx_std_20)
target_link_libraries(CS101AFinalProj PRIVATE SFML::Graphics)