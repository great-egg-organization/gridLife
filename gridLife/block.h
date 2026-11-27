#pragma once

#include "simConstants.h"
#include <array>
#include <atomic>
#include <immintrin.h>
#include <random>
#ifdef INTERACTIVE_SIM
#include <SFML/Graphics.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/System.hpp>
#include <SFML/System/Vector2.hpp>
#endif
#include "sfmlProxyTypes.h"

enum CellType : unsigned char { NONE, LEAF, BRANCH, SPROUT, ROOT, ANTENNA };

struct Gen {
  CellType left;
  CellType right;
  int nextGenSuccess;
  int nextGenFail;
};

struct Genome {
  std::atomic_uint ownerCount = 1;
  std::array<Gen, sc::GENOME_SIZE> genes;
  
  Genome() = default;
  Genome(Genome&);
  Genome& operator=(Genome&);
  Genome(Genome&&);
  Genome& operator=(Genome&&);
};

// 32 cells
struct CellCluster {
  union {
    __m256 energy[4];
    float lEnergy[32];
  };
  union {
    __m256 newEnergy[4];
    float lnewEnergy[32];
  };
  union {
    __m256i type;
    CellType lType[32];
  };
  union {
    __m256i parent;
    char lParent[32];
  };
  union {
    __m256i energyDir;
    char lEnergyDir[32];
  };
  union {
    __m256i timeToLive[4];
    uint32_t lTimeToLive[32];
  };
  union {
    __m256 soilEnergy[4];
    float lSoilEnergy[32];
  };
  union {
    __m256 soilOrganicMatter[4];
    float lSoilOrganicMatter[32];
  };
  union {
    __m256i updated;
    char lUpdated[32];
  };
};

constexpr sf::Vector2i offsetFromBitmask(char bitmask);
constexpr char oppositeDirectionBitmask(char bitmask);
constexpr char bitmaskFromOffset(sf::Vector2i offset);
bool mutateGenome(Gen* first, Gen* last, std::ranlux48 &);
sf::Vector2i wrapToBlockDim(sf::Vector2i);

struct AutomaticRerunSystem;

struct Block {
public:
  Block(AutomaticRerunSystem*);

  void step();
#ifdef INTERACTIVE_SIM
  void render(sf::RenderTarget *);
#endif

  void kill(sf::Vector2i pos);

  [[nodiscard]] bool verifyGridIntegrity() const;
  
  [[nodiscard]] CellCluster &getCluster(sf::Vector2i pos);
  [[nodiscard]] const CellCluster &getCluster(sf::Vector2i pos) const;

private:
  std::array<CellCluster, sc::BLOCK_CELL_COUNT / 32> cells;
  std::ranlux48 randomState;
  AutomaticRerunSystem* autoRerunSys;
  std::array<uint, sc::BLOCK_CELL_COUNT> cellNextGenGrid;
  std::array<uint, sc::BLOCK_CELL_COUNT> cellGenomeIdxGrid;
  std::vector<Genome> genomes;

  void update32Cells(sf::Vector2i first);

  void do32CellEnergyTransfers(sf::Vector2i first, __m256i updateMask);

  void updateSprout(sf::Vector2i pos);
  void update32Leafs(sf::Vector2i first, __m256i leafMask);
  void update32Roots(sf::Vector2i first, __m256i rootMask);
  void update32Antennas(sf::Vector2i first, __m256i antennaMask);
  
  void redirectEnergyToParent(sf::Vector2i pos, char parent);
};
