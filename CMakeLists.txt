project(texture_packer)
cmake_minimum_required(VERSION 2.8)

set(SOURCES
    main.cpp
    TexturePacker.h
    TexturePacker.cpp
)

find_package(Qt5Core)
find_package(Qt5Gui)
set(CMAKE_AUTOMOC ON)
add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Gui
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
