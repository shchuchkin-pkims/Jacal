add_library(jackal_core STATIC
    types.cpp
    game_state.cpp
    rules.cpp
    game.cpp
    ai.cpp
    map_def.cpp
)

target_include_directories(jackal_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(jackal_core PUBLIC cxx_std_17)

# No Qt dependency — pure C++
