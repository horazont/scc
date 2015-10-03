cmake_minimum_required(VERSION 2.8.11)

set(CMAKE_INCLUDE_CURRENT_DIRS ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5Widgets REQUIRED)
find_package(OpenGL REQUIRED)
find_package(epoxy REQUIRED)
find_package(SIGC++ REQUIRED)
find_package(Protobuf REQUIRED)

set(GAME_SRC
  application.cpp
  main.cpp
  mainmenu.cpp
  mode.cpp
  openglscene.cpp
  preferencesdialog.cpp
  mousebinding.cpp
  actionbutton.cpp
  bindings.cpp
  terraform/brush.cpp
  terraform/drag.cpp
  terraform/terraform.cpp
  terraform/terratool.cpp
  )

set(GAME_HEADERS
  fixups.hpp
  brush.proto
  )

set(GAME_UIS
    ui/application.ui
    ui/openglscene.ui
    ui/mainmenu.ui
    ui/terraform.ui
    ui/preferencesdialog.ui)

QT5_WRAP_UI(GAME_UI_HDRS ${GAME_UIS})

set(GAME_TEXTURE_RESOURCES
  resources/textures/grass00.png
  resources/textures/rock00.png
  resources/textures/blend00.png
  resources/textures/waves00.png
  resources/textures/sand00.png
  )

set(GAME_SHADER_RESOURCES
  resources/shaders/debug/graph_edge.frag
  resources/shaders/debug/graph_edge.vert
  resources/shaders/debug/graph_node.frag
  resources/shaders/debug/graph_node.vert
  resources/shaders/generic/normal_debug.frag
  resources/shaders/terraform/drag_plane.vert
  resources/shaders/terraform/fancy_drag_plane.frag
  resources/shaders/terraform/fluidsource.frag
  resources/shaders/terraform/fluidsource.vert
  resources/shaders/terraform/terrain_drag_plane.frag
  resources/shaders/terrain/normal_debug.geom
  resources/shaders/terrain/main.vert
  resources/shaders/terrain/main.geom
  resources/shaders/terrain/main.frag
  resources/shaders/terrain/test_overlay.frag
  resources/shaders/terrain/pointer_overlay.frag
  resources/shaders/terrain/brush_overlay.frag
  resources/shaders/overlay/main.vert
  resources/shaders/overlay/main.frag
  resources/shaders/fluid/main.frag
  resources/shaders/fluid/main.vert
  resources/shaders/fluid/main.geom
  resources/shaders/fluid/cpu.frag
  resources/shaders/fluid/cpu.vert
  resources/shaders/frustum/main.vert
  resources/shaders/frustum/main.frag
  resources/shaders/aabb/main.frag
  resources/shaders/aabb/main.vert
  resources/shaders/oct_sphere/main.frag
  resources/shaders/oct_sphere/main.vert
  resources/shaders/curve/main.frag
  resources/shaders/curve/main.vert
  resources/shaders/lib/universal_shader.frag
  resources/shaders/lib/cubehelix.frag
  resources/shaders/lib/matrix_block.glsl
  resources/shaders/lib/inv_matrix_block.glsl
  resources/shaders/lib/sunlight.frag
  resources/shaders/lib/unproject.glsl
  )

set(GAME_BRUSH_RESOURCES
  resources/brushes/test.pbrush
  )

set(GAME_STYLESHEET_RESOURCES
  resources/stylesheets/main.css
  )

add_custom_target(
    game-resources
    DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/shaders.rcc
    ${CMAKE_CURRENT_BINARY_DIR}/textures.rcc
    ${CMAKE_CURRENT_BINARY_DIR}/brushes.rcc
    ${CMAKE_CURRENT_BINARY_DIR}/stylesheets.rcc
    SOURCES
    resources/shaders.qrc
    ${GAME_SHADER_RESOURCES}
    resources/textures.qrc
    ${GAME_TEXTURE_RESOURCES}
    resources/brushes.qrc
    ${GAME_BRUSH_RESOURCES}
    resources/stylesheets.qrc
    ${GAME_STYLESHEET_RESOURCES}
    ${GAME_UIS})

function(add_resource_pack PACKNAME QRC SOURCES)
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${PACKNAME}.rcc
    COMMAND rcc-qt5 "${CMAKE_CURRENT_SOURCE_DIR}/${QRC}" --binary -o "${CMAKE_CURRENT_BINARY_DIR}/${PACKNAME}.rcc"
    DEPENDS ${SOURCES} ${QRC}
    COMMENT "Pack ${PACKNAME} resources"
  )
endfunction(add_resource_pack)

add_resource_pack("textures" "resources/textures.qrc" "${GAME_TEXTURE_RESOURCES}")
add_resource_pack("shaders" "resources/shaders.qrc" "${GAME_SHADER_RESOURCES}")
add_resource_pack("brushes" "resources/brushes.qrc" "${GAME_BRUSH_RESOURCES}")
add_resource_pack("stylesheets" "resources/stylesheets.qrc" "${GAME_STYLESHEET_RESOURCES}")

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${OPENGL_INCLUDE_DIR})
include_directories(${PROTOBUF_INCLUDE_DIRS})

PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS brush.proto)

add_executable(game ${GAME_SRC} ${GAME_HEADERS} ${PROTO_SRCS} ${PROTO_HDRS} ${GAME_UI_HDRS})
setup_scc_target(game)

target_link_libraries(game Qt5::Widgets)
target_link_libraries(game ffengine-core)
target_link_libraries(game ffengine-render)
target_link_libraries(game ffengine-sim)
target_link_libraries(game sigc++)
target_link_libraries(game ${PROTOBUF_LIBRARIES})
target_link_libraries(game ${OPENGL_LIBRARIES})
target_link_libraries(game epoxy)
target_link_libraries(game "atomic")

add_dependencies(game game-resources)
