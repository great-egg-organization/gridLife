add_executable(gridLife "main.cpp" "block.cpp" "automaticRerunSystem.cpp")

if (interactive_sim)
	target_link_libraries(gridLife SFML::System SFML::Window SFML::Graphics)
	target_link_libraries(gridLife GLEW::GLEW)
endif()
