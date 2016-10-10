set(EXEC_player lm_player.bin)
set(SRCS_player player.c)

set(EXEC_fade_white lm_fade_white.bin)
set(SRCS_fade_white fade_white.c)

set(EXEC_fade lm_fade.bin)
set(SRCS_fade fade.c)
set(EXEC_testpattern lm_testpattern.bin)
set(SRCS_testpattern testpattern.c)

add_executable(${EXEC_player} ${SRCS_player})
target_link_libraries(${EXEC_player} ${HELLO_PI_LIBS})

add_executable(${EXEC_fade_white} ${SRCS_fade_white})
target_link_libraries(${EXEC_fade_white} ${HELLO_PI_LIBS})

add_executable(${EXEC_fade} ${SRCS_fade})
target_link_libraries(${EXEC_fade} ${HELLO_PI_LIBS})

add_executable(${EXEC_testpattern} ${SRCS_testpattern})
target_link_libraries(${EXEC_testpattern} ${HELLO_PI_LIBS})

install(TARGETS ${EXEC_fade} ${EXEC_fade_white} ${EXEC_testpattern} ${EXEC_player}
        RUNTIME DESTINATION bin)
