add_executable(my_jump_adventure 
    MyJumpAdventure.c
    player.c
    utils.c
    )

target_include_directories(my_jump_adventure PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
target_link_libraries(my_jump_adventure PRIVATE Tilengine)

# Keep the existing relative resource path when running from this directory.
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../assets/my_jump_adventure"
     DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/assets")
