cmake_minimum_required(VERSION 2.8.11)

set(CMAKE_INCLUDE_CURRENT_DIRS ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5Widgets REQUIRED)
find_package(Qt5Qml REQUIRED)
find_package(Qt5Quick REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)


set(GAME_SRC
  application.cpp
  main.cpp
  mainmenu.cpp
  mode.cpp
  quickglscene.cpp
  terraform.cpp
  )

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${OPENGL_INCLUDE_DIR})
include_directories(${GLEW_INCLUDE_DIR})

add_executable(game ${GAME_SRC})
target_link_libraries(game Qt5::Widgets)
target_link_libraries(game Qt5::Quick)
target_link_libraries(game Qt5::Qml)
target_link_libraries(game engine)
target_link_libraries(game ${OPENGL_LIBRARIES})
target_link_libraries(game ${GLEW_LIBRARIES})
