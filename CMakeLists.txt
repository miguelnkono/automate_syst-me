cmake_minimum_required(VERSION 3.21)

project(system_automate)

add_executable(${PROJECT_NAME})
set_property(TARGET ${PROJECT_NAME} PROPERTY 
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)
target_sources(${PROJECT_NAME} PRIVATE main.c 
    automate_io.c
    data_structure.c
    pipeline.c)
target_include_directories(${PROJECT_NAME} PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
