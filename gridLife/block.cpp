#include "block.h"
#include "simConstants.h"
#include "automaticRerunSystem.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <immintrin.h>
#include <math.h>
#include <memory.h>
#include <random>


Genome::Genome(Genome& g){
  ownerCount = +g.ownerCount;
  memcpy(genes.data(), g.genes.data(), genes.size()*sizeof(Gen));
}

Genome& Genome::operator=(Genome& other){
  new(this) Genome(other);
  return *this;
}

Genome::Genome(Genome&& g){
  ownerCount = +g.ownerCount;
  genes = std::move(g.genes);
}

Genome& Genome::operator=(Genome&& other){
  new(this) Genome(other);
  return *this;
}

constexpr sf::Vector2i offsetFromBitmask(char bitmask) {
  constexpr auto hashmap = [] {
    constexpr size_t count = std::max(std::max(sc::ABOVEBIT, sc::BELOWBIT), std::max(sc::LEFTBIT, sc::RIGHTBIT)) + 1;
    std::array<sf::Vector2i, count> result{};
    result[sc::ABOVEBIT] = sf::Vector2i{0, 1};
    result[sc::BELOWBIT] = sf::Vector2i{0, -1};
    result[sc::LEFTBIT] = sf::Vector2i{-1, 0};
    result[sc::RIGHTBIT] = sf::Vector2i{1, 0};
    return result;
  }();

  assert(__builtin_popcount(bitmask) == 1);

  return hashmap[bitmask];
}

constexpr char oppositeDirectionBitmask(char bitmask) {
  constexpr auto hashmap = [] {
    constexpr size_t count = sc::ABOVEBIT + sc::BELOWBIT + sc::LEFTBIT + sc::RIGHTBIT + 1;
    std::array<char, count> result{};
    for (size_t i = 0; i < count; ++i) {
      if (i & sc::ABOVEBIT) {
        result[i] |= sc::BELOWBIT;
      }
      if (i & sc::BELOWBIT) {
        result[i] |= sc::ABOVEBIT;
      }
      if (i & sc::LEFTBIT) {
        result[i] |= sc::RIGHTBIT;
      }
      if (i & sc::RIGHTBIT) {
        result[i] |= sc::LEFTBIT;
      }
    }
    return result;
  }();

  return hashmap[bitmask];
}

// Assuming 'src_256' contains 32 unsigned 8-bit integers
static void expand_32_bytes_unsigned(__m256i src_256, __m256i &out0, __m256i &out1, __m256i &out2, __m256i &out3) {

  __m128i low_128 = _mm256_castsi256_si128(src_256);
  __m128i high_128 = _mm256_extracti128_si256(src_256, 1);

  // Lower Lane Conversions
  out0 = _mm256_cvtepu8_epi32(low_128);
  out1 = _mm256_cvtepu8_epi32(_mm_srli_si128(low_128, 8));

  // Upper Lane Conversions
  out2 = _mm256_cvtepu8_epi32(high_128);
  out3 = _mm256_cvtepu8_epi32(_mm_srli_si128(high_128, 8));
}

// Assuming 'src_256' contains 32 signed 8-bit integers
static void expand_32_bytes_signed(__m256i src_256, __m256i &out0,
                                   __m256i &out1, __m256i &out2,
                                   __m256i &out3) {

  __m128i low_128 = _mm256_castsi256_si128(src_256);
  __m128i high_128 = _mm256_extracti128_si256(src_256, 1);

  // Lower Lane Conversions
  out0 = _mm256_cvtepi8_epi32(low_128);
  out1 = _mm256_cvtepi8_epi32(_mm_srli_si128(low_128, 8));

  // Upper Lane Conversions
  out2 = _mm256_cvtepi8_epi32(high_128);
  out3 = _mm256_cvtepi8_epi32(_mm_srli_si128(high_128, 8));
}

constexpr char bitmaskFromOffset(sf::Vector2i offset) {
  if (offset.x == 0) {
    if (offset.y == 1) {
      return sc::ABOVEBIT;
    }
    if (offset.y == -1) {
      return sc::BELOWBIT;
    }
  }
  if (offset.y == 0) {
    if (offset.x == 1) {
      return sc::RIGHTBIT;
    }
    if (offset.x == -1) {
      return sc::LEFTBIT;
    }
  }

	assert(false);
	return 0;
}

bool mutateGenome(Gen* first, Gen* last, std::ranlux48 &engine) {
  float P_PER_LEVEL = std::exp(std::log(sc::MUTATION_RATE) * std::log(2) / std::ceil(std::log(sc::GENOME_SIZE) - 1));
  std::bernoulli_distribution rDist(P_PER_LEVEL);

  if (std::distance(first, last) == 1) {
    if (rDist(engine)) {
      constexpr CellType POSSIBILITIES[5] = {NONE, LEAF, SPROUT, ROOT, ANTENNA};
      std::uniform_int_distribution<unsigned char> cellTypeDist(0, 4);
      std::uniform_int_distribution<unsigned char> jumpDist(0, sc::GENOME_SIZE-1);
      first->left = POSSIBILITIES[cellTypeDist(engine)];
      first->right = POSSIBILITIES[cellTypeDist(engine)];
      first->nextGenSuccess = jumpDist(engine);
      first->nextGenFail = jumpDist(engine);
      return true;
    }
    return false;
  }

  bool ret = false;
  if (rDist(engine)) {
    ret = mutateGenome(first + std::distance(first, last) / 2, last, engine);
  }
  if (rDist(engine)) {
    ret = ret || mutateGenome(first, last - std::distance(first, last) / 2, engine);
  }

  return ret;
}

sf::Vector2i wrapToBlockDim(sf::Vector2i vec){
  vec.x += sc::BLOCK_DIM.x;
  vec.x %= sc::BLOCK_DIM.x;
  vec.y += sc::BLOCK_DIM.y;
  vec.y %= sc::BLOCK_DIM.y;
  
  return vec;
}

Block::Block(AutomaticRerunSystem* rerunSys) : randomState(2), autoRerunSys(rerunSys) {
  memset(cells.data(), 0, cells.size() * sizeof(cells[0]));
  memset(cellNextGenGrid.data(), 0, cellNextGenGrid.size() * sizeof(cellNextGenGrid[0]));
  memset(cellGenomeIdxGrid.data(), 0, cellGenomeIdxGrid.size() * sizeof(cellGenomeIdxGrid[0]));
  
  Genome sampleGenome;
  std::uniform_int_distribution<unsigned char> cellTypeDist(0, 4);
  std::uniform_int_distribution<unsigned char> jumpDist(0, sc::GENOME_SIZE-1);
  constexpr CellType POSSIBILITIES[5] = {NONE, LEAF, SPROUT, ROOT, ANTENNA};
  for (int i=0; i < sc::GENOME_SIZE; ++i){
    sampleGenome.genes[i] = Gen(POSSIBILITIES[cellTypeDist(randomState)], POSSIBILITIES[cellTypeDist(randomState)], jumpDist(randomState), jumpDist(randomState));
  }
  genomes.emplace_back(std::move(sampleGenome));
  CellCluster& cluster = getCluster(sc::BLOCK_DIM/2);
  cluster.lType[(sc::BLOCK_DIM.x/2)%32] = SPROUT;
  cluster.lEnergy[(sc::BLOCK_DIM.x/2)%32] = 999.0f;
  cluster.lTimeToLive[(sc::BLOCK_DIM.x/2)%32] = 200;
}

#ifdef NDEBUG
#define ADD_CHECK(x) (void)0
#else
#define ADD_CHECK(x) autoRerunSys->addCheck(x)
#endif

void Block::step() {
  size_t elementCount = cells.size();
  CellCluster *data = cells.data();
  __m256i ZERO = _mm256_setzero_si256();
  for (size_t i = 0; i < elementCount; ++i) {
    cells[i].updated = ZERO;
    for (int j=0; j < 4; ++j){
      cells[i].newEnergy[j] = cells[i].energy[j];
    }
  }
  
  for (int y = 0; y < sc::BLOCK_DIM.y; ++y) {
    for (int x = 0; x < sc::BLOCK_DIM.x; x += 32) {
      update32Cells(sf::Vector2i{x, y});
      ADD_CHECK([this](){return verifyGridIntegrity();});
    }
  }
  
  for (size_t i = 0; i < elementCount; ++i) {
    for (int j=0; j < 4; ++j){
      cells[i].energy[j] = cells[i].newEnergy[j];
    }
  }
  
  ADD_CHECK(verifyGridIntegrity());
}

static inline __m256 approachAVX2(__m256 s, __m256 m, __m256 t, __m256 x) {
  // max(min(tx+s,m),tx-s)
	
	__m256 xMult = _mm256_mul_ps(x, t);
  __m256 txPlusS = _mm256_add_ps(xMult, s);
  __m256 txMinS = _mm256_sub_ps(xMult, s);
  __m256 min = _mm256_min_ps(txPlusS, m);
  return _mm256_max_ps(min, txMinS);
}

void Block::update32Cells(sf::Vector2i pos) {
  CellCluster &cluster = getCluster(pos);

  __m256i toUpdate = _mm256_cmpgt_epi8(cluster.type, cluster.updated);

  {
    __m256 s = _mm256_set1_ps(sc::SOIL_ENERGY_TICK_CHANGE_AMOUNT);
    __m256 m = _mm256_set1_ps(sc::NORMAL_SOIL_ENERGY_LEVEL);
    __m256 t = _mm256_set1_ps(sc::SOIL_ENERGY_TICK_DECAY_FACTOR);

    cluster.soilEnergy[0] = approachAVX2(s, m, t, cluster.soilEnergy[0]);
    cluster.soilEnergy[1] = approachAVX2(s, m, t, cluster.soilEnergy[1]);
    cluster.soilEnergy[2] = approachAVX2(s, m, t, cluster.soilEnergy[2]);
    cluster.soilEnergy[3] = approachAVX2(s, m, t, cluster.soilEnergy[3]);
  }
  ADD_CHECK([this](){return verifyGridIntegrity();});

  if (_mm256_testz_si256(toUpdate, toUpdate)) {
    return;
  }
  
  {
    __m256 s = _mm256_set1_ps(sc::CELL_TICK_ENERGY_CONSUMPTION);
    __m256 m = _mm256_setzero_ps();
    __m256 t = _mm256_set1_ps(sc::CELL_TICK_ENERGY_DECAY_FACTOR);
    
    cluster.energy[0] = approachAVX2(s, m, t, cluster.energy[0]);
    cluster.energy[1] = approachAVX2(s, m, t, cluster.energy[1]);
    cluster.energy[2] = approachAVX2(s, m, t, cluster.energy[2]);
    cluster.energy[3] = approachAVX2(s, m, t, cluster.energy[3]);
  }

  int updateMask = _mm256_movemask_epi8(toUpdate);
  __m256i updateMasksLoop[4];
  expand_32_bytes_signed(toUpdate, updateMasksLoop[0], updateMasksLoop[1], updateMasksLoop[2], updateMasksLoop[3]);

  __m256i _BRANCH = _mm256_set1_epi8(BRANCH);
  int branchMask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(cluster.type, _BRANCH));

  __m256i ZERO = _mm256_setzero_si256();
  __m256i ONE = _mm256_set1_epi32(1);

  for (int i = 0; i < 4; ++i) {
    cluster.timeToLive[i] = _mm256_sub_epi32(cluster.timeToLive[i], _mm256_blendv_epi8(ZERO, ONE, updateMasksLoop[i]));

    __m256i noMoreTimeToLive = _mm256_and_si256(_mm256_cmpeq_epi32(cluster.timeToLive[i], ZERO), updateMasksLoop[i]);
    int movedMaskTTL =_mm256_movemask_ps(_mm256_castsi256_ps(noMoreTimeToLive));

    __m256 deathByOrganicMatter = _mm256_and_ps(_mm256_cmp_ps(cluster.soilOrganicMatter[i],_mm256_set1_ps(sc::LETHAL_ORGANIC_MATTER), _CMP_GT_OQ),_mm256_castsi256_ps(updateMasksLoop[i]));
    int movedMaskOM = _mm256_movemask_ps(deathByOrganicMatter);

    __m256 deathBySoilEnergy = _mm256_and_ps(_mm256_cmp_ps(cluster.soilEnergy[i],_mm256_set1_ps(sc::LETHAL_SOIL_ENERGY), _CMP_GT_OQ),_mm256_castsi256_ps(updateMasksLoop[i]));
    int movedMaskSE = _mm256_movemask_ps(deathBySoilEnergy);

    int doomedMask = movedMaskTTL | movedMaskOM | movedMaskSE;

    while (doomedMask) {
      ADD_CHECK([&](){return verifyGridIntegrity();});
      kill(pos + sf::Vector2i(__builtin_ctz(doomedMask) + 8 * i, 0));
      ADD_CHECK([&](){return verifyGridIntegrity();});
      doomedMask &= doomedMask - 1;
    }
  }
  
  toUpdate = _mm256_cmpgt_epi8(cluster.type, cluster.updated);
  __m256i sproutMask = _mm256_cmpeq_epi8(cluster.type, _mm256_set1_epi8(SPROUT));
  int movedUSM = _mm256_movemask_epi8(_mm256_and_si256(sproutMask, toUpdate));

  while (movedUSM) {
    sf::Vector2i _pos = pos + sf::Vector2i(__builtin_ctz(movedUSM), 0);

    updateSprout(_pos);
    ADD_CHECK([this](){return verifyGridIntegrity();});

    movedUSM &= movedUSM - 1;
    toUpdate = _mm256_cmpgt_epi8(cluster.type, cluster.updated);
		movedUSM &= _mm256_movemask_epi8(toUpdate);
  }
  
  __m256i leafMask = _mm256_and_si256(_mm256_cmpeq_epi8(cluster.type, _mm256_set1_epi8(LEAF)), toUpdate);
  __m256i rootMask = _mm256_and_si256(_mm256_cmpeq_epi8(cluster.type, _mm256_set1_epi8(ROOT)), toUpdate);
  __m256i antennaMask = _mm256_and_si256(_mm256_cmpeq_epi8(cluster.type, _mm256_set1_epi8(ANTENNA)), toUpdate);

  update32Leafs(pos, leafMask);
  ADD_CHECK([this](){return verifyGridIntegrity();});
  update32Roots(pos, rootMask);
  ADD_CHECK([this](){return verifyGridIntegrity();});
  update32Antennas(pos, antennaMask);
  ADD_CHECK([this](){return verifyGridIntegrity();});

  do32CellEnergyTransfers(pos, _mm256_andnot_si256(sproutMask, toUpdate));
  ADD_CHECK([this](){return verifyGridIntegrity();});

	cluster.updated = _mm256_set1_epi8(sc::UPDATED_FLAG);
}

void Block::do32CellEnergyTransfers(sf::Vector2i pos, __m256i updateMask) {
  if (_mm256_testz_si256(updateMask, updateMask)) {
    return;
  }

  CellCluster &cells = getCluster(pos);
  CellCluster &cellsAbove = getCluster(pos + sf::Vector2i{0, 1});
  CellCluster &cellsBelow = getCluster(pos + sf::Vector2i{0, -1});

  __m256i ZERO = _mm256_setzero_si256();
  
  cells.energyDir = _mm256_blendv_epi8(cells.energyDir, cells.parent, _mm256_and_si256(_mm256_cmpeq_epi8(cells.energyDir, ZERO), updateMask));
  int movedMaskNoEnergyDir = _mm256_movemask_epi8(_mm256_and_si256(updateMask, _mm256_cmpeq_epi8(cells.energyDir, ZERO)));
  while (movedMaskNoEnergyDir) {
    ADD_CHECK([&](){return verifyGridIntegrity();});
    kill(pos + sf::Vector2i(__builtin_ctz(movedMaskNoEnergyDir), 0));
    ADD_CHECK([&](){return verifyGridIntegrity();});
    movedMaskNoEnergyDir &= movedMaskNoEnergyDir - 1;
  }

  __m256i energyTransferBitmasks = _mm256_blendv_epi8(ZERO, cells.energyDir, updateMask);

  // 32 bit integers
  __m256i energyTransferCounts[4];
  {
    // popcount(energyTransferBitmask)
    __m256i EverySecondBit = _mm256_set1_epi8(0b0101);
    __m256i EverySecondGroupOf2Bits = _mm256_set1_epi8(0b0011);
    __m256i n = energyTransferBitmasks;
    n = _mm256_sub_epi8(n, _mm256_and_si256(_mm256_srli_epi32(n, 1), EverySecondBit));
    __m256i epu8Result = _mm256_add_epi8(_mm256_and_si256(n, EverySecondGroupOf2Bits), _mm256_and_si256(_mm256_srli_epi32(n, 2), EverySecondGroupOf2Bits));
    expand_32_bytes_unsigned(epu8Result, energyTransferCounts[0],energyTransferCounts[1], energyTransferCounts[2], energyTransferCounts[3]);
  }

  // 32 bit floats eqaual to 1 for transfer and 0 for no transfer arranged in
  // ((above, below, left, right), ...)
  __m256 energyTransferMasks[16];
  {
    __m256i ABOVEBIT = _mm256_set1_epi8(sc::ABOVEBIT);
    __m256i BELOWBIT = _mm256_set1_epi8(sc::BELOWBIT);
    __m256i LEFTBIT = _mm256_set1_epi8(sc::LEFTBIT);
    __m256i RIGHTBIT = _mm256_set1_epi8(sc::RIGHTBIT);
    __m256i ONE = _mm256_set1_epi8(1);

    __m256i epu8ResultAbove = _mm256_srli_epi32(_mm256_and_si256(ABOVEBIT, energyTransferBitmasks), __builtin_ctz(sc::ABOVEBIT));
    __m256i epu8ResultBelow = _mm256_srli_epi32(_mm256_and_si256(BELOWBIT, energyTransferBitmasks), __builtin_ctz(sc::BELOWBIT));
    __m256i epu8ResultLeft = _mm256_srli_epi32(_mm256_and_si256(LEFTBIT, energyTransferBitmasks), __builtin_ctz(sc::LEFTBIT));
    __m256i epu8ResultRight = _mm256_srli_epi32(_mm256_and_si256(RIGHTBIT, energyTransferBitmasks), __builtin_ctz(sc::RIGHTBIT));

    __m256i *masks = (__m256i *)energyTransferMasks;
    expand_32_bytes_unsigned(epu8ResultAbove, masks[0], masks[4], masks[8], masks[12]);
    expand_32_bytes_unsigned(epu8ResultBelow, masks[1], masks[5], masks[9], masks[13]);
    expand_32_bytes_unsigned(epu8ResultLeft, masks[2], masks[6], masks[10], masks[14]);
    expand_32_bytes_unsigned(epu8ResultRight, masks[3], masks[7], masks[11], masks[15]);

    for (__m256 &mask : energyTransferMasks) {
      mask = _mm256_cvtepi32_ps(_mm256_castps_si256(mask));
    }
  }

  for (int i = 0; i < 4; ++i) {
    __m256 countFloat = _mm256_cvtepi32_ps(energyTransferCounts[i]);
    __m256 energyTransferAmounts = _mm256_blendv_ps(_mm256_div_ps(cells.energy[i], countFloat), _mm256_setzero_ps(), _mm256_cmp_ps(countFloat, _mm256_setzero_ps(), _CMP_EQ_OS));
    cells.newEnergy[i] = _mm256_sub_ps(cells.newEnergy[i], _mm256_mul_ps(countFloat, energyTransferAmounts));

    // above
    {
      __m256 maskedEnergyTransferAmounts = _mm256_mul_ps(energyTransferAmounts, energyTransferMasks[i * 4]);
      cellsAbove.newEnergy[i] = _mm256_add_ps(maskedEnergyTransferAmounts, cellsAbove.newEnergy[i]);
    }
    // below
    {
      __m256 maskedEnergyTransferAmounts = _mm256_mul_ps(energyTransferAmounts, energyTransferMasks[i * 4 + 1]);
      cellsBelow.newEnergy[i] = _mm256_add_ps(maskedEnergyTransferAmounts, cellsBelow.newEnergy[i]);
    }
    // left
    {
      __m256 maskedEnergyTransferAmounts = _mm256_mul_ps(energyTransferAmounts, energyTransferMasks[i * 4 + 2]);
      __m256 rightShifted = _mm256_loadu_ps((float *)&cells.newEnergy[i] - 1);
      if (i == 0) {
        int tmp = _mm256_extract_epi32(_mm256_castps_si256(maskedEnergyTransferAmounts), 0);
        CellCluster &lCluster = getCluster(pos + sf::Vector2i{-1, 0});
        ((float *)lCluster.newEnergy)[31] += *(float *)&tmp;
        maskedEnergyTransferAmounts = _mm256_castsi256_ps(_mm256_insert_epi32(_mm256_castps_si256(maskedEnergyTransferAmounts), 0, 0));
      }
      rightShifted = _mm256_add_ps(maskedEnergyTransferAmounts, rightShifted);
      _mm256_storeu_ps((float *)&cells.newEnergy[i] - 1, rightShifted);
    }
    // right
    {
      __m256 maskedEnergyTransferAmounts = _mm256_mul_ps(energyTransferAmounts, energyTransferMasks[i * 4 + 3]);
      __m256 leftShifted = _mm256_loadu_ps((float *)&cells.newEnergy[i] + 1);
      if (i == 3) {
        int tmp = _mm256_extract_epi32(_mm256_castps_si256(maskedEnergyTransferAmounts), 7);
        CellCluster &rCluster = getCluster(pos + sf::Vector2i{1, 0});
        ((float *)rCluster.newEnergy)[0] += *(float *)&tmp;
        maskedEnergyTransferAmounts = _mm256_castsi256_ps(_mm256_insert_epi32(_mm256_castps_si256(maskedEnergyTransferAmounts), 0, 7));
      }
      leftShifted = _mm256_add_ps(maskedEnergyTransferAmounts, leftShifted);
      _mm256_storeu_ps((float *)&cells.newEnergy[i] + 1, leftShifted);
    }
  }
}

void Block::update32Leafs(sf::Vector2i pos, __m256i leafMask) {
  __m256 leafEnergyGen = _mm256_set1_ps(sc::LEAF_TICK_ENERGY_GEN);

  __m256i expandedMasks[4];
  expand_32_bytes_signed(leafMask, expandedMasks[0], expandedMasks[1], expandedMasks[2], expandedMasks[3]);

  CellCluster &cells = getCluster(pos);

  for (int i = 0; i < 4; ++i) {
    __m256 leafEnergyGenMasked = _mm256_blendv_ps(_mm256_setzero_ps(), leafEnergyGen, _mm256_castsi256_ps(expandedMasks[i]));
    cells.energy[i] = _mm256_add_ps(cells.energy[i], leafEnergyGenMasked);
  }
}

void Block::updateSprout(sf::Vector2i pos) {
  ADD_CHECK([&](){return verifyGridIntegrity();});
  
  CellCluster& cluster = getCluster(pos);
  size_t sproutInd = pos.x + pos.y * sc::BLOCK_DIM.x;

  char parentBitmask = cluster.lParent[pos.x % 32];
  parentBitmask = std::max(parentBitmask, (char)1);
  sf::Vector2i forward = offsetFromBitmask(oppositeDirectionBitmask(parentBitmask));
  sf::Vector2i rightD = forward.perpendicular();

  sf::Vector2i rightP = wrapToBlockDim(pos + rightD);
  CellCluster *rClust;
  uint *rGenIdx;
  uint *rCGen;
  rClust = &getCluster(rightP);
  rGenIdx = &cellGenomeIdxGrid[rightP.x + sc::BLOCK_DIM.x * rightP.y];
  rCGen = &cellNextGenGrid[rightP.x + sc::BLOCK_DIM.x * rightP.y];

  sf::Vector2i leftP = wrapToBlockDim(pos - rightD);
  CellCluster *lClust;
  uint *lGenIdx;
  uint *lCGen;
  lClust = &getCluster(leftP);
  lGenIdx = &cellGenomeIdxGrid[leftP.x + sc::BLOCK_DIM.x * leftP.y];
  lCGen = &cellNextGenGrid[leftP.x + sc::BLOCK_DIM.x * leftP.y];

  sf::Vector2i frontP = wrapToBlockDim(pos + forward);
  uint *fGenIdx;
  uint *fCGen;
  CellCluster *fClust;
  fClust = &getCluster(frontP);
  fGenIdx = &cellGenomeIdxGrid[frontP.x + sc::BLOCK_DIM.x * frontP.y];
  fCGen = &cellNextGenGrid[frontP.x + sc::BLOCK_DIM.x * frontP.y];

  CellType &cellForwardType = fClust->lType[frontP.x % 32];
	
	ADD_CHECK(cellGenomeIdxGrid[sproutInd] < genomes.size());
  Gen currentGen = genomes[cellGenomeIdxGrid[sproutInd]].genes[cellNextGenGrid[sproutInd]];

  float genExecuteCost = sc::CELL_CREATE_COSTS[currentGen.left] + sc::CELL_CREATE_COSTS[currentGen.right] + sc::CELL_CREATE_COSTS[BRANCH];

  float &currentCellEnergy = cluster.lEnergy[pos.x%32];

  CellType &cellRightType = rClust->lType[rightP.x % 32];
  CellType &cellLeftType = lClust->lType[leftP.x % 32];

  if (currentCellEnergy < genExecuteCost || cellForwardType == BRANCH ||
      (cellRightType && (currentGen.right && currentGen.right != SPROUT)) || cellRightType == BRANCH ||
      (cellLeftType && (currentGen.left && currentGen.left != SPROUT)) || cellLeftType == BRANCH) {
    cellNextGenGrid[sproutInd] = currentGen.nextGenFail;
    return;
  }

  if (cellForwardType) {
    ADD_CHECK([&](){return verifyGridIntegrity();});
    kill(frontP);
    ADD_CHECK([&](){return verifyGridIntegrity();});
  }
  if (cellRightType && currentGen.right) {
    ADD_CHECK([&](){return verifyGridIntegrity();});
    kill(rightP);
    ADD_CHECK([&](){return verifyGridIntegrity();});
  }
  if (cellLeftType && currentGen.left) {
    ADD_CHECK([&](){return verifyGridIntegrity();});
    kill(leftP);
    ADD_CHECK([&](){return verifyGridIntegrity();});
  }

  fClust->lEnergy[frontP.x % 32] = currentCellEnergy - genExecuteCost;
  fClust->lnewEnergy[frontP.x % 32] = currentCellEnergy - genExecuteCost;
  fClust->lParent[frontP.x % 32] = parentBitmask;
  size_t nextGenInd = currentGen.nextGenSuccess;
  *fCGen = nextGenInd;
  *fGenIdx = cellGenomeIdxGrid[sproutInd];
  fClust->lTimeToLive[frontP.x % 32] = sc::CELL_LIFETIME;
  cellForwardType = SPROUT;
  fClust->lUpdated[frontP.x % 32] = sc::UPDATED_FLAG;

  cluster.lType[pos.x % 32] = BRANCH;
  currentCellEnergy = sc::CELL_INITIAL_ENERGY;
  cluster.lEnergyDir[pos.x % 32] = oppositeDirectionBitmask(parentBitmask);

  if (currentGen.right) {
    cellRightType = currentGen.right;
    rClust->lEnergy[rightP.x % 32] = sc::CELL_INITIAL_ENERGY;
    rClust->lnewEnergy[rightP.x % 32] = sc::CELL_INITIAL_ENERGY;
    rClust->lParent[rightP.x % 32] = bitmaskFromOffset(-rightD);
    rClust->lTimeToLive[rightP.x % 32] = sc::CELL_LIFETIME;
    rClust->lUpdated[rightP.x % 32] = sc::UPDATED_FLAG;
    if (currentGen.right == SPROUT) {
      cluster.lEnergyDir[pos.x % 32] |= bitmaskFromOffset(rightD);
      rClust->lEnergyDir[rightP.x%32] = 0;
      *rGenIdx = cellGenomeIdxGrid[sproutInd];
      *rCGen = nextGenInd;

      Genome nGen = genomes[cellGenomeIdxGrid[sproutInd]];
      nGen.ownerCount = 1;
      if (mutateGenome(nGen.genes.data(), nGen.genes.data() + nGen.genes.size(), randomState)) {
        genomes.emplace_back(std::move(nGen));
      } else {
        // TODO
        //++genomes[cellGenomeIdxGrid[sproutInd]].ownerCount;
      }
    } else {
      rClust->lEnergyDir[rightP.x % 32] = bitmaskFromOffset(-rightD);
    }
  }
  if (currentGen.left) {
    cellLeftType = currentGen.left;
    lClust->lEnergy[leftP.x % 32] = sc::CELL_INITIAL_ENERGY;
    lClust->lnewEnergy[leftP.x % 32] = sc::CELL_INITIAL_ENERGY;
    lClust->lParent[leftP.x % 32] = bitmaskFromOffset(rightD);
    lClust->lTimeToLive[leftP.x % 32] = sc::CELL_LIFETIME;
    lClust->lUpdated[leftP.x % 32] = sc::UPDATED_FLAG;
    if (currentGen.left == SPROUT) {
      cluster.lEnergyDir[pos.x % 32] |= bitmaskFromOffset(-rightD);
      lClust->lEnergyDir[leftP.x%32] = 0;
      *lGenIdx = cellGenomeIdxGrid[sproutInd];
      *lCGen = nextGenInd;

      Genome nGen = genomes[cellGenomeIdxGrid[sproutInd]];
      nGen.ownerCount = 1;
      if (mutateGenome(nGen.genes.data(), nGen.genes.data() + nGen.genes.size(), randomState)) {
        genomes.emplace_back(std::move(nGen));
      } else {
        // TODO
        //++genomes[cellGenomeIdxGrid[sproutInd]].ownerCount;
      }
    } else {
    	lClust->lEnergyDir[leftP.x % 32] = bitmaskFromOffset(rightD);
    }
  }
}

void Block::update32Roots(sf::Vector2i pos, __m256i rootMask) {
  CellCluster &cluster = getCluster(pos);

  __m256 depletionFraction = _mm256_set1_ps(sc::ROOT_SOIL_TICK_DEPLETION_FRACTION);
  __m256 efficiency = _mm256_set1_ps(sc::ROOT_EFFICIENCY);

  __m256 expandedMasks[4];
  __m256i *_masks = (__m256i *)expandedMasks;
  expand_32_bytes_signed(rootMask, _masks[0], _masks[1], _masks[2], _masks[3]);

  for (int i = 0; i < 4; ++i) {
    __m256 depletionFractionMasked = _mm256_blendv_ps(_mm256_setzero_ps(), depletionFraction, expandedMasks[i]);
    __m256 depletionAmount = _mm256_mul_ps(depletionFractionMasked, cluster.soilOrganicMatter[i]);

    cluster.soilOrganicMatter[i] = _mm256_sub_ps(cluster.soilOrganicMatter[i], depletionAmount);

    cluster.energy[i] = _mm256_add_ps(cluster.energy[i], _mm256_mul_ps(efficiency, depletionAmount));
  }
}

void Block::update32Antennas(sf::Vector2i pos, __m256i antennaMask) {
  CellCluster &cluster = getCluster(pos);

  __m256 depletionFraction = _mm256_set1_ps(sc::ANTENNA_SOIL_TICK_DEPLETION_FRACTION);
  __m256 efficiency = _mm256_set1_ps(sc::ANTENNA_EFFICIENCY);

  __m256 expandedMasks[4];
  __m256i *_masks = (__m256i *)expandedMasks;
  expand_32_bytes_signed(antennaMask, _masks[0], _masks[1], _masks[2], _masks[3]);

  for (int i = 0; i < 4; ++i) {
    __m256 depletionFractionMasked = _mm256_blendv_ps(_mm256_setzero_ps(), depletionFraction, expandedMasks[i]);
    __m256 depletionAmount = _mm256_mul_ps(depletionFractionMasked, cluster.soilEnergy[i]);

    cluster.soilEnergy[i] = _mm256_sub_ps(cluster.soilEnergy[i], depletionAmount);

    cluster.energy[i] = _mm256_add_ps(cluster.energy[i], _mm256_mul_ps(efficiency, depletionAmount));
  }
}

void Block::kill(sf::Vector2i pos) {
  CellType &selfType = ((CellType *)&getCluster(pos).type)[pos.x % 32];
  CellType persistentSelfType = selfType;

  assert(selfType);

  if (selfType == SPROUT && cellGenomeIdxGrid[pos.x + pos.y * sc::BLOCK_DIM.x] < genomes.size()) {
    // TODO
    //--sim->genomes[cellGenomeIdxGrid[pos.x+pos.y*sc::BLOCK_DIM.x]].ownerCount;
  }

  selfType = NONE;
  char persistentParent = ((char *)&getCluster(pos).parent)[pos.x % 32];
  getCluster(pos).lEnergyDir[pos.x % 32] = 0;
  getCluster(pos).lParent[pos.x % 32] = 0;
  getCluster(pos).lSoilEnergy[pos.x % 32] += sc::CELL_DEATH_BASE_ENERGY + sc::CELL_DEATH_ENERGY_INTO_SOIL_FRACTION * getCluster(pos).lEnergy[pos.x%32];
  getCluster(pos).lSoilOrganicMatter[pos.x % 32] += sc::CELL_DEATH_ORGANIC_MATTER;

  constexpr sf::Vector2i surroundOffs[9] = {{-1, 1},  {0, 1},  {1, 1}, {-1, 0},  {0, 0},  {1, 0}, {-1, -1}, {0, -1}, {1, -1}};
  float sum = 0;
  for (const sf::Vector2i &o : surroundOffs) {
    sf::Vector2i _pos = wrapToBlockDim(pos + o);
    sum += getCluster(_pos).lSoilOrganicMatter[_pos.x % 32];
  }
  float avg = sum / 9;
  for (const sf::Vector2i &o : surroundOffs) {
    sf::Vector2i _pos = pos + o;
    getCluster(_pos).lSoilOrganicMatter[_pos.x % 32] = avg;
  }
  
  if (persistentSelfType == SPROUT && persistentParent){
    redirectEnergyToParent(pos, persistentParent);
  }
  
  // possibly redundant
  constexpr sf::Vector2i adjacentOffs[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
  for (const sf::Vector2i &o : adjacentOffs){
    sf::Vector2i _pos = wrapToBlockDim(pos + o);
    CellCluster& clust = getCluster(_pos);
    clust.lEnergyDir[_pos.x%32] &= ~bitmaskFromOffset(-o);
    clust.lParent[_pos.x%32] &= ~bitmaskFromOffset(-o);
  }
}

void Block::redirectEnergyToParent(sf::Vector2i pos, char parent){
#ifndef NDEBUG
  size_t counter = 0;
#endif
  while (parent) {
    pos = wrapToBlockDim(pos + offsetFromBitmask(parent));
		char& energyDir = getCluster(pos).lEnergyDir[pos.x%32];
		energyDir &= ~oppositeDirectionBitmask(parent);
		if (!energyDir){
			parent = getCluster(pos).lParent[pos.x% 32];
	  	energyDir = parent;
		}
		else {
		  break;
		}
#ifndef NDEBUG
		++counter;
		ADD_CHECK(counter < sc::BLOCK_CELL_COUNT);
#endif
  }
}

bool Block::verifyGridIntegrity() const {
  for (int x = 0; x < sc::BLOCK_DIM.x; ++x) {
    for (int y = 0; y < sc::BLOCK_DIM.y; ++y) {
      sf::Vector2i pos(x, y);
      CellType selfType = ((CellType *)&getCluster(pos).type)[pos.x % 32];
      char parent = ((char *)&getCluster(pos).parent)[pos.x % 32];

      if (selfType == SPROUT) {
        if (cellGenomeIdxGrid[x + y * sc::BLOCK_DIM.x] >= genomes.size() && ((char *)&getCluster(pos).updated)[pos.x % 32] != sc::UPDATED_FLAG){
          return false;
        }
      }

      if(__builtin_popcount(parent) > 1){
        return false;
      }
      if (selfType && ((int *)getCluster(pos).timeToLive)[pos.x % 32] < 0){
        return false;
      }
      char energyDirs = ((char *)&getCluster(pos).energyDir)[pos.x % 32];
      while (energyDirs) {
        char next = energyDirs & (energyDirs - 1);
        sf::Vector2i targetPos = wrapToBlockDim(pos + offsetFromBitmask(energyDirs ^ next));
        CellType targetCellType;
        targetCellType = ((CellType *)&getCluster(targetPos).type)[targetPos.x % 32];
        if(targetCellType != BRANCH && ((targetCellType != SPROUT && targetCellType != BRANCH) || selfType != BRANCH)){
          return false;
        }
        energyDirs = next;
      }
      if (parent) {
        sf::Vector2i parentPos = wrapToBlockDim(pos + offsetFromBitmask(parent));
        CellType targetCellType;
        targetCellType = ((CellType *)&getCluster(parentPos).type)[parentPos.x % 32];
        if(targetCellType != BRANCH){
          return false;
        }
      }
      if(std::isnan(((float *)getCluster(pos).energy)[pos.x % 32])){
        return false;
      }
    }
  }
  return true;
}

#ifdef INTERACTIVE_SIM
static void appendTriangle(sf::VertexArray &vertices, sf::Vector2f a, sf::Vector2f b, sf::Vector2f c, sf::Color color) {
  vertices.append(sf::Vertex{a, color});
  vertices.append(sf::Vertex{b, color});
  vertices.append(sf::Vertex{c, color});
}

static void appendRectangle(sf::VertexArray &vertices, sf::Vector2f position, sf::Vector2f size, sf::Color color) {
  appendTriangle(vertices, position, position + sf::Vector2f{size.x, 0.f}, position + size, color);
  appendTriangle(vertices, position, position + size, position + sf::Vector2f{0.f, size.y}, color);
}

static void appendEllipse(sf::VertexArray &vertices, sf::Vector2f position, float radius, sf::Vector2f scale, int pointCount, sf::Color color) {
  constexpr float PI = 3.141592654f;
  sf::Vector2f center = position + sf::Vector2f{radius * scale.x, radius * scale.y};
  for (int i = 0; i < pointCount; ++i) {
    float angleA = i * 2 * PI / pointCount - PI / 2;
    float angleB = (i + 1) * 2 * PI / pointCount - PI / 2;
    sf::Vector2f a = center + sf::Vector2f{std::cos(angleA) * radius * scale.x, std::sin(angleA) * radius * scale.y};
    sf::Vector2f b = center + sf::Vector2f{std::cos(angleB) * radius * scale.x, std::sin(angleB) * radius * scale.y};
    appendTriangle(vertices, center, a, b, color);
  }
}

CellCluster &Block::getCluster(sf::Vector2i pos) {
  pos = wrapToBlockDim(pos);
  assert(pos.x > -1 && pos.y > -1);
  return cells[(pos.x + pos.y * sc::BLOCK_DIM.x) / 32];
}

const CellCluster &Block::getCluster(sf::Vector2i pos) const {
  pos = wrapToBlockDim(pos);
  assert(pos.x > -1 && pos.y > -1);
  return cells[(pos.x + pos.y * sc::BLOCK_DIM.x) / 32];
}

void Block::render(sf::RenderTarget *target) {
  sf::VertexArray vertices(sf::PrimitiveType::Triangles);
  sf::FloatRect viewRect = sf::FloatRect(target->getView().getCenter() - 0.5f * target->getView().getSize(), target->getView().getSize());
  sf::IntRect relevantCells;
  relevantCells.position = sf::Vector2i{std::max(0, (int)(viewRect.position.x - 1)), std::max(0, (int)(viewRect.position.y - 1))};
  relevantCells.size = sf::Vector2i{
		std::min(sc::BLOCK_DIM.x - relevantCells.position.x,(int)(viewRect.position.x + viewRect.size.x + 1)),
		std::min((int)sc::BLOCK_DIM.y - relevantCells.position.y, (int)(viewRect.position.y + viewRect.size.y + 1))
	};
	
	for (int x = relevantCells.position.x; x < relevantCells.position.x + relevantCells.size.x; ++x) {
    for (int y = relevantCells.position.y; y < relevantCells.position.y + relevantCells.size.y; ++y) {
    	sf::Vector2i pos{x, y};
      float soilEnergy = getCluster(pos).lSoilEnergy[pos.x % 32];
      float soilOrganicMatter = getCluster(pos).lSoilOrganicMatter[pos.x % 32];
      std::uint8_t soilEnergyLevel = (std::uint8_t)(255.0f * std::clamp((soilEnergy - sc::NORMAL_SOIL_ENERGY_LEVEL) / (sc::LETHAL_SOIL_ENERGY - sc::NORMAL_SOIL_ENERGY_LEVEL), 0.0f, 1.0f));
      std::uint8_t organicMatterLevel = (std::uint8_t)(255.0f * std::min(soilOrganicMatter / sc::LETHAL_ORGANIC_MATTER, 1.0f));
      appendRectangle(vertices, sf::Vector2f((float)x, (float)y), sf::Vector2f(1.f, 1.f), sf::Color(organicMatterLevel, soilEnergyLevel, 0));
    }
	}

  for (int x = relevantCells.position.x; x < relevantCells.position.x + relevantCells.size.x; ++x) {
    for (int y = relevantCells.position.y; y < relevantCells.position.y + relevantCells.size.y; ++y) {
      sf::Vector2i pos{x, y};
      CellType &type = getCluster(pos).lType[pos.x % 32];

      if (!type) {
        continue;
      }

      float intensity = getCluster(pos).lEnergy[pos.x%32] * 20.0f + 20.0f;
      sf::Color branchColor(intensity, intensity, intensity);

      for (int i = 0; i < 4; ++i) {
        if (((char *)&getCluster(pos).energyDir)[pos.x % 32] & (1 << i)) {
          constexpr float CONNECTION_THICKNESS = 0.7f;
          sf::Vector2f ctp(x + 0.5f, y + 0.5f);
          sf::Vector2f off = (sf::Vector2f)offsetFromBitmask(1 << i);
          appendTriangle(vertices,
            -0.5f * CONNECTION_THICKNESS * off.perpendicular() + ctp,
            off + ctp,
            0.5f * CONNECTION_THICKNESS * off.perpendicular() + ctp,
            branchColor);
        }
      }

      switch (type) {
      case NONE:
        break;
      case LEAF: {
        sf::Vector2f off = (sf::Vector2f)offsetFromBitmask(std::max<char>(((char *)&getCluster(pos).parent)[pos.x % 32], 1));
        sf::Vector2f tmp{std::abs(off.x) * 0.5f, std::abs(off.y) * 0.5f};
        sf::Vector2f leafPos = sf::Vector2f{(float)x, (float)y} + sf::Vector2f{std::abs(tmp.perpendicular().x), std::abs(tmp.perpendicular().y)} * 0.5f;
        appendEllipse(vertices, leafPos, 0.5f, tmp + sf::Vector2f{0.5f, 0.5f}, 12, sf::Color::Green);
        break;
      }
      case BRANCH:
				break;
      case SPROUT: {
        float brightness = getCluster(pos).lEnergy[pos.x%32]*0.001f/sc::CELL_CREATE_COSTS[BRANCH];
        appendEllipse(vertices, sf::Vector2f(x, y), 0.5f, sf::Vector2f{1.f, 1.f}, 12, sf::Color(255, 255, 255, 255 * brightness));
        break;
      }
      case ROOT: {
        appendRectangle(vertices, sf::Vector2f((x + 0.15f), (y + 0.15f)), sf::Vector2f(0.7f, 0.7f), sf::Color(0x8c, 0x5b, 0x32));
        break;
      }
      case ANTENNA: {
        appendEllipse(vertices, sf::Vector2f(x, y), 0.35f, sf::Vector2f{1.f, 1.f}, 12, sf::Color::Blue);
        break;
      }
      }
    }
  }

  target->draw(vertices);
}
#endif
