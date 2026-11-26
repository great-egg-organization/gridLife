add_executable(gridLife "main.cpp" "block.cpp" "automaticRerunSystem.cpp")

# TODO remove dependencies when interactive_sim unset
target_link_libraries(gridLife SFML::System SFML::Window SFML::Graphics)

