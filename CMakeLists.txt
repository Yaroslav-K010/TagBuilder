cmake_minimum_required(VERSION 3.16)
project(TagBuilder LANGUAGES CXX)

if(WIN32)
    if(NOT DEFINED CMAKE_PREFIX_PATH AND EXISTS "C:/Qt/6.8.3/mingw_64")
        set(CMAKE_PREFIX_PATH "C:/Qt/6.8.3/mingw_64")
    endif()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(SOURCES
        Src/main.cpp
        Src/Valve.cpp
        Src/DeviceFactory.cpp
        Src/DeviceManager.cpp
        Src/MainWindow.cpp
        Src/TagBuilder.qrc
)

set(HEADERS
        Src/Device.h
        Src/Valve.h
        Src/DeviceFactory.h
        Src/DeviceManager.h
        Src/MainWindow.h
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Src)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets)

if(WIN32)
   find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt HINTS "${CMAKE_PREFIX_PATH}/bin" PATH_SUFFIXES bin)
   if(WINDEPLOYQT_EXECUTABLE)
       add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
           COMMAND "${WINDEPLOYQT_EXECUTABLE}" --compiler-runtime "$<TARGET_FILE:${PROJECT_NAME}>"
           COMMENT "Deploying Qt and MinGW runtime libraries"
       )
   endif()
endif()

