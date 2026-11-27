#pragma once
#include <cstddef>
#include <cstdint>
#include "sfmlProxyTypes.h"

namespace sc{
	// mutations are done by recursively subdividing the genome with probability p=NUMERATOR/2^DENOMINATOR_LENGTH to continue at each step
	// the real probability of mutating any specific genome should then be p^log2(GENOME_SIZE) (However it isn't because determining semantic properties of programs is hard)
	constexpr int MUTATION_RATE_PER_LEVEL_NUMERATOR = 1;
	constexpr int MUTATION_RATE_PER_LEVEL_DENOMINATOR_LENGTH = 3;
	
	constexpr size_t GENOME_SIZE = 1 << 5; // must be a power of 2
	constexpr uint32_t CELL_LIFETIME = 80;
	constexpr float CELL_TICK_ENERGY_CONSUMPTION = 0.0f;
	constexpr float CELL_TICK_ENERGY_DECAY_FACTOR = 1.0f;
	constexpr float LEAF_TICK_BASE_ENERGY_GEN = 5.5f; // interpolation based on organic matter, max at lethal
	constexpr float LEAF_TICK_MAX_ENERGY_GEN = 18.0f;
	constexpr float ROOT_SOIL_TICK_DEPLETION_FRACTION = 0.05f;
	constexpr float ROOT_EFFICIENCY = 1.2f;
	constexpr float ANTENNA_SOIL_TICK_DEPLETION_FRACTION = 0.05f;
	constexpr float ANTENNA_EFFICIENCY = 1.2f;
	constexpr float SPROUT_ENERGY_EXPENDITURE_FRACTION = 0.5f; // fraction of the sprout's available energy consumed to execute its genome
	constexpr float LIFETIME_PER_ENERGY_EXPENDED[6] = {0, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}; // NONE, LEAF, BRANCH, SPROUT, ROOT, ANTENNA
	constexpr float CELL_CREATE_COSTS[6] = {0, 500, 80, 80, 500, 300}; // NONE, LEAF, BRANCH, SPROUT, ROOT, ANTENNA
	constexpr float CELL_INITIAL_ENERGY = 10;
	constexpr float LETHAL_ORGANIC_MATTER = 1200;
	constexpr float NORMAL_SOIL_ENERGY_LEVEL = 800;
	constexpr float LETHAL_SOIL_ENERGY = 1200;
	constexpr float CELL_DEATH_BASE_ENERGY = 60;
	constexpr float CELL_DEATH_ORGANIC_MATTER = 60;
	constexpr float CELL_DEATH_ENERGY_INTO_SOIL_ENERGY_FRACTION = 0.9f;
	constexpr float CELL_DEATH_ENERGY_INTO_ORGANIC_MATTER_FRACTION = 0.01f;
	constexpr float SOIL_ENERGY_TICK_CHANGE_AMOUNT = 1.0f;
	constexpr float SOIL_ENERGY_TICK_DECAY_FACTOR = 0.99f;
	
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
