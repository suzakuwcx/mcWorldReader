find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(Boost REQUIRED COMPONENTS python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR} REQUIRED)
find_package(Boost REQUIRED COMPONENTS iostreams)

set(CMAKE_SHARED_LIBRARY_PREFIX "")

add_library(canvil SHARED anvil.cpp utils.cpp)
target_include_directories(canvil PUBLIC include)
target_link_libraries(canvil PUBLIC Boost::python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR} Boost::iostreams Python3::Python nbt++)
add_custom_command(
    TARGET canvil
    POST_BUILD
    COMMAND cp -f $<TARGET_FILE:canvil> ${PROJECT_BINARY_DIR}
)

add_python_source(world.py)
