cmake_minimum_required(VERSION 2.8.11)

set(CMAKE_INCLUDE_CURRENT_DIRS ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5Widgets REQUIRED)
find_package(Qt5Qml REQUIRED)
find_package(Qt5Quick REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(SIGC++ REQUIRED)


set(GAME_SRC
  application.cpp
  main.cpp
  mainmenu.cpp
  mode.cpp
  quickglscene.cpp
  terraform.cpp
  )

set(GAME_HEADERS
  fixups.hpp
  )

set(GAME_QML_RESOURCES
  resources/qml/main.qml
  resources/qml/MainForm.ui.qml
  resources/qml/Terra.qml
  resources/qml/TerraForm.ui.qml
  resources/qml/MainMenu.qml
  resources/qml/MainMenuForm.ui.qml
  )

set(GAME_TEXTURE_RESOURCES
  resources/textures/grass00.png
  )

add_custom_target(game-resources)

function(add_resource_pack PACKNAME QRC SOURCES)
  add_custom_target(
    game-resource-${PACKNAME}
    COMMAND rcc "${CMAKE_CURRENT_SOURCE_DIR}/${QRC}" -binary -o "${PACKNAME}.rcc"
    SOURCES ${SOURCES} ${QRC}
  )
  add_dependencies(game-resources game-resource-${PACKNAME})
endfunction(add_resource_pack)

add_resource_pack("qml" "resources/qml.qrc" "${GAME_QML_RESOURCES}")
add_resource_pack("textures" "resources/textures.qrc" "${GAME_TEXTURE_RESOURCES}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${OPENGL_INCLUDE_DIR})
include_directories(${GLEW_INCLUDE_DIR})

add_executable(game ${GAME_SRC} ${GAME_HEADERS})
target_link_libraries(game Qt5::Widgets)
target_link_libraries(game Qt5::Quick)
target_link_libraries(game Qt5::Qml)
target_link_libraries(game engine)
target_link_libraries(game sigc++)
target_link_libraries(game ${OPENGL_LIBRARIES})
target_link_libraries(game ${GLEW_LIBRARIES})
add_dependencies(game game-resources)
