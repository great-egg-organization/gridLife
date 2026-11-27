add_executable(gridLife "main.cpp" "block.cpp" "automaticRerunSystem.cpp")

# TODO remove dependencies when interactive_sim unset
if (interactive_sim)
	target_link_libraries(gridLife SFML::System SFML::Window SFML::Graphics)
endif()
