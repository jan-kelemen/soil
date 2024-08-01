add_custom_target(assets
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/coast_sand_rocks_02_diff_1k.jpg
        ${CMAKE_CURRENT_SOURCE_DIR}/coast_sand_rocks_02_nor_gl_1k.jpg
        ${CMAKE_CURRENT_SOURCE_DIR}/heightmap.jpg
        ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/coast_sand_rocks_02_diff_1k.jpg
        ${CMAKE_CURRENT_SOURCE_DIR}/coast_sand_rocks_02_nor_gl_1k.jpg
        ${CMAKE_CURRENT_SOURCE_DIR}/heightmap.jpg
)