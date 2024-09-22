cmake_minimum_required(VERSION 3.28.3)

project(mcworldreader)

function(add_python_source source)
    file(RELATIVE_PATH relative_source ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${source})
    string(REPLACE "/" "-" tgt ${relative_source})
    add_custom_target(${tgt} ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${source})
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${source}
                       COMMAND cp -f ${CMAKE_CURRENT_SOURCE_DIR}/${source} ${CMAKE_CURRENT_BINARY_DIR}/${source}
                       MAIN_DEPENDENCY ${source})
endfunction(add_python_source)

add_subdirectory(3rdparty)
add_subdirectory(src)
