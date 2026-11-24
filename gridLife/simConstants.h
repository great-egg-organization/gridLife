#pragma once
#include <cstdint>
#include <SFML/System.hpp>

namespace sc{
	constexpr float MUTATION_RATE = 0.04f;
	constexpr size_t GENOME_SIZE = 1 << 5;
	constexpr uint32_t CELL_LIFETIME = 80;
	constexpr float LEAF_TICK_ENERGY_GEN = 10.0f;
	constexpr float ROOT_SOIL_TICK_DEPLETION_FRACTION = 0.2f;
	constexpr float ROOT_EFFICIENCY = 0.9f;
	constexpr float ANTENNA_SOIL_TICK_DEPLETION_FRACTION = 0.2f;
	constexpr float ANTENNA_EFFICIENCY = 0.9f;
	constexpr float CELL_CREATE_COSTS[6] = {0, 80, 80, 80, 80, 80};
	constexpr float CELL_INITIAL_ENERGY = 10;
	constexpr float LETHAL_ORGANIC_MATTER = 4000;
	constexpr float LETHAL_SOIL_ENERGY = 800;
	constexpr float CELL_DEATH_BASE_ENERGY = 60;
	constexpr float CELL_DEATH_ORGANIC_MATTER = 60;
	constexpr float CELL_DEATH_ENERGY_INTO_SOIL_FRACTION = 0.01f;
	constexpr float NORMAL_SOIL_ENERGY_LEVEL = 400;
	constexpr float SOIL_ENERGY_TICK_CHANGE_AMOUNT = 4.0;
	constexpr float SOIL_ENERGY_TICK_DECAY_FACTOR = 0.98f;
	
	enum Direction : char {
		ABOVEBIT = 1,
		BELOWBIT = 2,
		LEFTBIT = 4,
		RIGHTBIT = 8,
	};
	
	constexpr char UPDATED_FLAG = 127;
	
	constexpr sf::Vector2i BLOCK_DIM = sf::Vector2i{512, 512};
	constexpr size_t BLOCK_CELL_COUNT = BLOCK_DIM.x*BLOCK_DIM.y;
}
