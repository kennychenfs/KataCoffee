#include "../neuralnet/nninputs.h"

using namespace std;
bool NNInputs::historyChannelWithDirection = false;

int NNPos::xydToPos(int x, int y, Direction dir, int nnXLen, int nnYLen) {
  return (int)dir * nnXLen * nnYLen + (y * nnXLen + x);
}
int NNPos::locToPos(Loc loc, int boardXSize, int nnXLen, int nnYLen) {
  if(loc.spot == Board::NULL_LOC || loc.dir == D_NONE)
    return nnXLen * nnYLen * NUM_ACTUAL_DIRECTIONS;
  return loc.dir * nnXLen * nnYLen + Location::getY(loc.spot, boardXSize) * nnXLen +
         Location::getX(loc.spot, boardXSize);
}
Loc NNPos::posToLoc(int pos, int boardXSize, int boardYSize, int nnXLen, int nnYLen) {
  if(pos == nnXLen * nnYLen * NUM_ACTUAL_DIRECTIONS)
    return Loc(Board::NULL_LOC, D_NONE);
  int dir = pos / (nnXLen * nnYLen);
  pos /= nnXLen * nnYLen;
  int x = pos % nnXLen;
  int y = pos / nnXLen;
  if(x < 0 || x >= boardXSize || y < 0 || y >= boardYSize)
    return Loc(Board::NULL_LOC, D_NONE);
  return Loc(Location::getSpot(x, y, boardXSize), dir);
}

// only for ownership map and NNInputs::fillRowV1

int NNPos::xyToPos(int x, int y, int nnXLen) {
  return y * nnXLen + x;
}
int NNPos::spotToPos(Spot spot, int boardXSize, int nnXLen, int nnYLen) {
  if(spot == Board::NULL_LOC)
    return nnXLen * nnYLen;
  return Location::getY(spot, boardXSize) * nnXLen + Location::getX(spot, boardXSize);
}
Spot NNPos::PosToSpot(int pos, int boardXSize, int boardYSize, int nnXLen, int nnYLen) {
  if(pos == nnXLen * nnYLen)
    return Board::NULL_LOC;
  int x = pos % nnXLen;
  int y = pos / nnXLen;
  if(x < 0 || x >= boardXSize || y < 0 || y >= boardYSize)
    return Board::NULL_LOC;
  return Location::getSpot(x, y, boardXSize);
}

int NNPos::getPolicySize(int nnXLen, int nnYLen) {
  return nnXLen * nnYLen * NUM_ACTUAL_DIRECTIONS;  // exclude D_NONE
}

//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------

const Hash128 MiscNNInputParams::ZOBRIST_PLAYOUT_DOUBLINGS = Hash128(0xa5e6114d380bfc1dULL, 0x4160557f1222f4adULL);
const Hash128 MiscNNInputParams::ZOBRIST_NN_POLICY_TEMP = Hash128(0xebcbdfeec6f4334bULL, 0xb85e43ee243b5ad2ULL);
const Hash128 MiscNNInputParams::ZOBRIST_POLICY_OPTIMISM = Hash128(0x88415c85c2801955ULL, 0x39bdf76b2aaa5eb1ULL);

//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------

double ScoreValue::whiteWinsOfWinner(Player winner) {
  if(winner == P_WHITE)
    return 1.0;
  else if(winner == P_BLACK)
    return 0.0;

  assert(false);
}

NNOutput::NNOutput() : whiteOwnerMap(NULL), noisedPolicyProbs(NULL) {}
NNOutput::NNOutput(const NNOutput& other) {
  nnHash = other.nnHash;
  whiteWinProb = other.whiteWinProb;
  whiteLossProb = other.whiteLossProb;
  varTimeLeft = other.varTimeLeft;
  shorttermWinlossError = other.shorttermWinlossError;

  nnXLen = other.nnXLen;
  nnYLen = other.nnYLen;
  if(other.whiteOwnerMap != NULL) {
    whiteOwnerMap = new float[nnXLen * nnYLen];
    std::copy(other.whiteOwnerMap, other.whiteOwnerMap + nnXLen * nnYLen, whiteOwnerMap);
  } else
    whiteOwnerMap = NULL;

  if(other.noisedPolicyProbs != NULL) {
    noisedPolicyProbs = new float[NNPos::MAX_NN_POLICY_SIZE];
    std::copy(other.noisedPolicyProbs, other.noisedPolicyProbs + NNPos::MAX_NN_POLICY_SIZE, noisedPolicyProbs);
  } else
    noisedPolicyProbs = NULL;

  std::copy(other.policyProbs, other.policyProbs + NNPos::MAX_NN_POLICY_SIZE, policyProbs);
}

NNOutput::NNOutput(const vector<shared_ptr<NNOutput>>& others) {
  assert(others.size() < 1000000);
  int len = (int)others.size();
  float floatLen = (float)len;
  assert(len > 0);
  for(int i = 1; i < len; i++) {
    assert(others[i]->nnHash == others[0]->nnHash);
  }
  nnHash = others[0]->nnHash;

  whiteWinProb = 0.0f;
  whiteLossProb = 0.0f;
  varTimeLeft = 0.0f;
  shorttermWinlossError = 0.0f;
  for(int i = 0; i < len; i++) {
    const NNOutput& other = *(others[i]);
    whiteWinProb += other.whiteWinProb;
    whiteLossProb += other.whiteLossProb;
    varTimeLeft += other.varTimeLeft;
    shorttermWinlossError += other.shorttermWinlossError;
  }
  whiteWinProb /= floatLen;
  whiteLossProb /= floatLen;
  varTimeLeft /= floatLen;
  shorttermWinlossError /= floatLen;

  nnXLen = others[0]->nnXLen;
  nnYLen = others[0]->nnYLen;

  {
    float whiteOwnerMapCount = 0.0f;
    whiteOwnerMap = NULL;
    for(int i = 0; i < len; i++) {
      const NNOutput& other = *(others[i]);
      if(other.whiteOwnerMap != NULL) {
        if(whiteOwnerMap == NULL) {
          whiteOwnerMap = new float[nnXLen * nnYLen];
          std::fill(whiteOwnerMap, whiteOwnerMap + nnXLen * nnYLen, 0.0f);
        }
        whiteOwnerMapCount += 1.0f;
        for(int pos = 0; pos < nnXLen * nnYLen; pos++)
          whiteOwnerMap[pos] += other.whiteOwnerMap[pos];
      }
    }
    if(whiteOwnerMap != NULL) {
      assert(whiteOwnerMapCount > 0);
      for(int pos = 0; pos < nnXLen * nnYLen; pos++)
        whiteOwnerMap[pos] /= whiteOwnerMapCount;
    }
  }

  noisedPolicyProbs = NULL;

  // For technical correctness in case of impossibly rare hash collisions:
  // Just give up if they don't all match in move legality
  {
    bool mismatch = false;
    std::fill(policyProbs, policyProbs + NNPos::MAX_NN_POLICY_SIZE, 0.0f);
    for(int i = 0; i < len; i++) {
      const NNOutput& other = *(others[i]);
      for(int pos = 0; pos < NNPos::MAX_NN_POLICY_SIZE; pos++) {
        if(i > 0 && (policyProbs[pos] < 0) != (other.policyProbs[pos] < 0))  // negative policy = illegal move
          mismatch = true;
        policyProbs[pos] += other.policyProbs[pos];
      }
    }
    // In case of illegal moves mismatch, just take the first one
    // This should basically never happen, only on true hash collisions
    if(mismatch) {
      const NNOutput& other = *(others[0]);
      std::copy(other.policyProbs, other.policyProbs + NNPos::MAX_NN_POLICY_SIZE, policyProbs);
    } else {
      for(int pos = 0; pos < NNPos::MAX_NN_POLICY_SIZE; pos++)
        policyProbs[pos] /= floatLen;
    }
  }
}

NNOutput& NNOutput::operator=(const NNOutput& other) {
  if(&other == this)
    return *this;
  nnHash = other.nnHash;
  whiteWinProb = other.whiteWinProb;
  whiteLossProb = other.whiteLossProb;
  varTimeLeft = other.varTimeLeft;
  shorttermWinlossError = other.shorttermWinlossError;

  nnXLen = other.nnXLen;
  nnYLen = other.nnYLen;
  if(whiteOwnerMap != NULL)
    delete[] whiteOwnerMap;
  if(other.whiteOwnerMap != NULL) {
    whiteOwnerMap = new float[nnXLen * nnYLen];
    std::copy(other.whiteOwnerMap, other.whiteOwnerMap + nnXLen * nnYLen, whiteOwnerMap);
  } else
    whiteOwnerMap = NULL;
  if(noisedPolicyProbs != NULL)
    delete[] noisedPolicyProbs;
  if(other.noisedPolicyProbs != NULL) {
    noisedPolicyProbs = new float[NNPos::MAX_NN_POLICY_SIZE];
    std::copy(other.noisedPolicyProbs, other.noisedPolicyProbs + NNPos::MAX_NN_POLICY_SIZE, noisedPolicyProbs);
  } else
    noisedPolicyProbs = NULL;

  std::copy(other.policyProbs, other.policyProbs + NNPos::MAX_NN_POLICY_SIZE, policyProbs);

  return *this;
}

NNOutput::~NNOutput() {
  if(whiteOwnerMap != NULL) {
    delete[] whiteOwnerMap;
    whiteOwnerMap = NULL;
  }
  if(noisedPolicyProbs != NULL) {
    delete[] noisedPolicyProbs;
    noisedPolicyProbs = NULL;
  }
}

void NNOutput::debugPrint(ostream& out, const Board& board) {
  out << "Win " << Global::strprintf("%.2fc", whiteWinProb * 100) << endl;
  out << "Loss " << Global::strprintf("%.2fc", whiteLossProb * 100) << endl;
  out << "VarTimeLeft " << Global::strprintf("%.1f", varTimeLeft) << endl;
  out << "STWinlossError " << Global::strprintf("%.2fc", shorttermWinlossError * 100) << endl;

  out << "Policy" << endl;
  for(int y = 0; y < board.y_size; y++) {
    for(int x = 0; x < board.x_size; x++) {
      for(Direction dir = 0; dir < NUM_DIRECTIONS - 1; ++dir) {
        int pos = NNPos::xydToPos(x, y, dir, nnXLen, nnYLen);
        float prob = policyProbs[pos];
        if(prob < 0)
          out << "   - ";
        else
          out << Global::strprintf("%4d ", (int)round(prob * 1000));
      }
      out << endl;
    }
    out << endl;
  }

  if(whiteOwnerMap != NULL) {
    for(int y = 0; y < board.y_size; y++) {
      for(int x = 0; x < board.x_size; x++) {
        int pos = NNPos::xyToPos(x, y, nnXLen);
        float whiteOwn = whiteOwnerMap[pos];
        out << Global::strprintf("%5d ", (int)round(whiteOwn * 1000));
      }
      out << endl;
    }
    out << endl;
  }
}

//-------------------------------------------------------------------------------------------------------------

static void copyWithSymmetry(
  const float* src,
  float* dst,
  int nSize,
  int hSize,
  int wSize,
  int cSize,
  bool useNHWC,
  int symmetry,
  bool reverse) {
  bool transpose = (symmetry & 0x4) != 0 && hSize == wSize;
  bool flipX = (symmetry & 0x2) != 0;
  bool flipY = (symmetry & 0x1) != 0;
  if(transpose && !reverse)
    std::swap(flipX, flipY);
  if(useNHWC) {
    int nStride = hSize * wSize * cSize;
    int hStride = wSize * cSize;
    int wStride = cSize;
    int hBaseNew = 0;
    int hStrideNew = hStride;
    int wBaseNew = 0;
    int wStrideNew = wStride;

    if(flipY) {
      hBaseNew = (hSize - 1) * hStrideNew;
      hStrideNew = -hStrideNew;
    }
    if(flipX) {
      wBaseNew = (wSize - 1) * wStrideNew;
      wStrideNew = -wStrideNew;
    }

    if(transpose)
      std::swap(hStrideNew, wStrideNew);

    for(int n = 0; n < nSize; n++) {
      for(int h = 0; h < hSize; h++) {
        int nhOld = n * nStride + h * hStride;
        int nhNew = n * nStride + hBaseNew + h * hStrideNew;
        for(int w = 0; w < wSize; w++) {
          int nhwOld = nhOld + w * wStride;
          int nhwNew = nhNew + wBaseNew + w * wStrideNew;
          for(int c = 0; c < cSize; c++) {
            dst[nhwNew + c] = src[nhwOld + c];
          }
        }
      }
    }
  } else {
    int ncSize = nSize * cSize;
    int ncStride = hSize * wSize;
    int hStride = wSize;
    int wStride = 1;
    int hBaseNew = 0;
    int hStrideNew = hStride;
    int wBaseNew = 0;
    int wStrideNew = wStride;

    if(flipY) {
      hBaseNew = (hSize - 1) * hStrideNew;
      hStrideNew = -hStrideNew;
    }
    if(flipX) {
      wBaseNew = (wSize - 1) * wStrideNew;
      wStrideNew = -wStrideNew;
    }

    if(transpose)
      std::swap(hStrideNew, wStrideNew);

    for(int nc = 0; nc < ncSize; nc++) {
      for(int h = 0; h < hSize; h++) {
        int nchOld = nc * ncStride + h * hStride;
        int nchNew = nc * ncStride + hBaseNew + h * hStrideNew;
        for(int w = 0; w < wSize; w++) {
          int nchwOld = nchOld + w * wStride;
          int nchwNew = nchNew + wBaseNew + w * wStrideNew;
          dst[nchwNew] = src[nchwOld];
        }
      }
    }
  }
}

void SymmetryHelpers::copyInputsWithSymmetry(
  const float* src,
  float* dst,
  int nSize,
  int hSize,
  int wSize,
  int cSize,
  bool useNHWC,
  int symmetry) {
  copyWithSymmetry(src, dst, nSize, hSize, wSize, cSize, useNHWC, symmetry, false);
}

void SymmetryHelpers::copyOutputsWithSymmetry(
  const float* src,
  float* dst,
  int nSize,
  int hSize,
  int wSize,
  int symmetry) {
  copyWithSymmetry(src, dst, nSize, hSize, wSize, 1, false, symmetry, true);
}

int SymmetryHelpers::invert(int symmetry) {
  if(symmetry == 5)
    return 6;
  if(symmetry == 6)
    return 5;
  return symmetry;
}

int SymmetryHelpers::compose(int firstSymmetry, int nextSymmetry) {
  if(isTranspose(firstSymmetry))
    nextSymmetry = (nextSymmetry & 0x4) | ((nextSymmetry & 0x2) >> 1) | ((nextSymmetry & 0x1) << 1);
  return firstSymmetry ^ nextSymmetry;
}

int SymmetryHelpers::compose(int firstSymmetry, int nextSymmetry, int nextNextSymmetry) {
  return compose(compose(firstSymmetry, nextSymmetry), nextNextSymmetry);
}

Spot SymmetryHelpers::getSymSpot(int x, int y, int xSize, int ySize, int symmetry) {
  bool transpose = (symmetry & 0x4) != 0;
  bool flipX = (symmetry & 0x2) != 0;
  bool flipY = (symmetry & 0x1) != 0;
  if(flipX) {
    x = xSize - x - 1;
  }
  if(flipY) {
    y = ySize - y - 1;
  }

  if(transpose)
    std::swap(x, y);
  return Location::getSpot(x, y, transpose ? ySize : xSize);
}

Spot SymmetryHelpers::getSymSpot(int x, int y, const Board& board, int symmetry) {
  return getSymSpot(x, y, board.x_size, board.y_size, symmetry);
}

Spot SymmetryHelpers::getSymSpot(Spot spot, const Board& board, int symmetry) {
  if(spot == Board::NULL_LOC)
    return spot;
  return getSymSpot(Location::getX(spot, board.x_size), Location::getY(spot, board.x_size), board, symmetry);
}

Spot SymmetryHelpers::getSymSpot(Spot spot, int xSize, int ySize, int symmetry) {
  if(spot == Board::NULL_LOC)
    return spot;
  return getSymSpot(Location::getX(spot, xSize), Location::getY(spot, xSize), xSize, ySize, symmetry);
}

Direction SymmetryHelpers::getSymDir(Direction dir, int symmetry) {
  assert(dir >= 0 && dir < NUM_DIRECTIONS && symmetry >= 0 && symmetry < 8);
  if(dir == D_NONE)
    return D_NONE;
  bool isTranspose = (symmetry & 0x4) != 0;
  bool isFlipX = (symmetry & 0x2) != 0;
  bool isFlipY = (symmetry & 0x1) != 0;
  if(isFlipX ^ isFlipY) {
    switch(dir) {
      case D_NORTHEAST:
        return D_NORTHWEST;
      case D_NORTHWEST:
        return D_NORTHEAST;
    }
  }
  if(isTranspose) {
    switch(dir) {
      case D_NORTH:
        return D_WEST;
      case D_WEST:
        return D_NORTH;
    }
  }
  assert(false);
}

Board SymmetryHelpers::getSymBoard(const Board& board, int symmetry) {
  bool transpose = (symmetry & 0x4) != 0;
  bool flipX = (symmetry & 0x2) != 0;
  bool flipY = (symmetry & 0x1) != 0;
  Board symBoard(transpose ? board.y_size : board.x_size, transpose ? board.x_size : board.y_size);
  for(int y = 0; y < board.y_size; y++) {
    for(int x = 0; x < board.x_size; x++) {
      Spot spot = Location::getSpot(x, y, board.x_size);
      int symX = flipX ? board.x_size - x - 1 : x;
      int symY = flipY ? board.y_size - y - 1 : y;
      if(transpose)
        std::swap(symX, symY);
      Spot symSpot = Location::getSpot(symX, symY, symBoard.x_size);
      bool suc = symBoard.setStone(symSpot, board.colors[spot]);
      assert(suc);
      (void)suc;
    }
  }
  return symBoard;
}

//-------------------------------------------------------------------------------------------------------------

static void setRowBin(float* rowBin, int pos, int feature, float value, int posStride, int featureStride) {
  rowBin[pos * posStride + feature * featureStride] = value;
}

// Currently does NOT depend on history
Hash128 NNInputs::getHash(
  const Board& board,
  const BoardHistory& hist,
  Player nextPlayer,
  const MiscNNInputParams& nnInputParams) {
  Hash128 hash = board.getSitHash(nextPlayer);

  // Fold in whether the game is over or not, since this affects how we compute input features
  // but is not a function necessarily of previous hashed values.
  // If the history is in a weird prolonged state, also treat it similarly.
  if(hist.isGameFinished)
    hash ^= Board::ZOBRIST_GAME_IS_OVER;

  // Fold in asymmetric playout indicator
  if(nnInputParams.playoutDoublingAdvantage != 0) {
    int64_t playoutDoublingsDiscretized = (int64_t)(nnInputParams.playoutDoublingAdvantage * 256.0f);
    hash.hash0 += Hash::splitMix64((uint64_t)playoutDoublingsDiscretized);
    hash.hash1 += Hash::basicLCong((uint64_t)playoutDoublingsDiscretized);
    hash ^= MiscNNInputParams::ZOBRIST_PLAYOUT_DOUBLINGS;
  }

  // Fold in policy temperature
  if(nnInputParams.nnPolicyTemperature != 1.0f) {
    int64_t nnPolicyTemperatureDiscretized = (int64_t)(nnInputParams.nnPolicyTemperature * 2048.0f);
    hash.hash0 ^= Hash::basicLCong2((uint64_t)nnPolicyTemperatureDiscretized);
    hash.hash1 = Hash::splitMix64(hash.hash1 + (uint64_t)nnPolicyTemperatureDiscretized);
    hash.hash0 += hash.hash1;
    hash ^= MiscNNInputParams::ZOBRIST_NN_POLICY_TEMP;
  }

  // Fold in policy optimism
  if(nnInputParams.policyOptimism > 0) {
    hash ^= MiscNNInputParams::ZOBRIST_POLICY_OPTIMISM;
    int64_t policyOptimismDiscretized = (int64_t)(nnInputParams.policyOptimism * 1024.0);
    hash.hash0 = Hash::rrmxmx(Hash::splitMix64(hash.hash0) + (uint64_t)policyOptimismDiscretized);
    hash.hash1 = Hash::rrmxmx(hash.hash1 + hash.hash0 + (uint64_t)policyOptimismDiscretized);
  }

  return hash;
}

//===========================================================================================
// INPUTSVERSION 1(edited V7 in KataGo)
//===========================================================================================

void NNInputs::fillRowV1(
  const Board& board,
  const BoardHistory& hist,
  Player nextPlayer,
  const MiscNNInputParams& nnInputParams,
  int nnXLen,
  int nnYLen,
  bool useNHWC,
  float* rowBin,
  float* rowGlobal) {
  assert(nnXLen <= NNPos::MAX_BOARD_LEN);
  assert(nnYLen <= NNPos::MAX_BOARD_LEN);
  assert(board.x_size <= nnXLen);
  assert(board.y_size <= nnYLen);
  std::fill(rowBin, rowBin + NUM_FEATURES_SPATIAL_V1 * nnXLen * nnYLen, 0.0f);
  std::fill(rowGlobal, rowGlobal + NUM_FEATURES_GLOBAL_V1, 0.0f);

  Player pla = nextPlayer;
  Player opp = getOpp(pla);
  int xSize = board.x_size;
  int ySize = board.y_size;

  int featureStride;
  int posStride;
  int currentFeatureIdx = 0;
  if(useNHWC) {
    featureStride = 1;
    posStride = NNInputs::NUM_FEATURES_SPATIAL_V1;
  } else {
    featureStride = nnXLen * nnYLen;
    posStride = 1;
  }

  for(int y = 0; y < ySize; y++) {
    for(int x = 0; x < xSize; x++) {
      int pos = NNPos::xyToPos(x, y, nnXLen);
      Spot spot = Location::getSpot(x, y, xSize);

      // Feature 0 - on board
      setRowBin(rowBin, pos, currentFeatureIdx, 1.0f, posStride, featureStride);

      Color stone = board.colors[spot];

      // Features 1,2 - pla,opp stone
      if(stone == pla)
        setRowBin(rowBin, pos, currentFeatureIdx + 1, 1.0f, posStride, featureStride);
      else if(stone == opp)
        setRowBin(rowBin, pos, currentFeatureIdx + 2, 1.0f, posStride, featureStride);
    }
  }
  currentFeatureIdx += 3;

  // Feature 3~6 - last move.
  if(hist.moveHistory.size() > 0) {
    Spot spot = hist.moveHistory[hist.moveHistory.size() - 1].loc.spot;
    int pos = NNPos::spotToPos(spot, board.x_size, nnXLen, nnYLen);
    Direction dir = hist.moveHistory[hist.moveHistory.size() - 1].loc.dir;
    setRowBin(rowBin, pos, currentFeatureIdx + dir, 1.0f, posStride, featureStride);
  }
  currentFeatureIdx += 4;
  int numTurnsOfHistoryIncluded = 0;

  // Features 7~10 or 7~ - last 2,3,4,5 moves
  const vector<Move>& moveHistory = hist.moveHistory;
  size_t moveHistoryLen = moveHistory.size();
  // Also effectively wipe history as we change phase
  assert(moveHistoryLen >= hist.numTurns);
  int numTurns = hist.numTurns;

  if(numTurns >= 2 && moveHistory[moveHistoryLen - 2].pla == pla) {
    Spot prev2Spot = moveHistory[moveHistoryLen - 2].loc.spot;
    Direction prev2dir = moveHistory[moveHistoryLen - 2].loc.dir;
    numTurnsOfHistoryIncluded = 1;
    if(prev2Spot != Board::NULL_LOC) {
      int pos = NNPos::spotToPos(prev2Spot, xSize, nnXLen, nnYLen);
      if(historyChannelWithDirection) {
        setRowBin(rowBin, pos, currentFeatureIdx + prev2dir, 1.0f, posStride, featureStride);
        currentFeatureIdx += 4;
      } else
        setRowBin(rowBin, pos, currentFeatureIdx++, 1.0f, posStride, featureStride);
    }
    if(numTurns >= 3 && moveHistory[moveHistoryLen - 3].pla == opp) {
      Spot prev3Spot = moveHistory[moveHistoryLen - 3].loc.spot;
      Direction prev3dir = moveHistory[moveHistoryLen - 3].loc.dir;
      numTurnsOfHistoryIncluded = 1;
      if(prev3Spot != Board::NULL_LOC) {
        int pos = NNPos::spotToPos(prev3Spot, xSize, nnXLen, nnYLen);
        if(historyChannelWithDirection) {
          setRowBin(rowBin, pos, currentFeatureIdx + prev3dir, 1.0f, posStride, featureStride);
          currentFeatureIdx += 4;
        } else
          setRowBin(rowBin, pos, currentFeatureIdx++, 1.0f, posStride, featureStride);
      }
      if(numTurns >= 4 && moveHistory[moveHistoryLen - 4].pla == pla) {
        Spot prev4Spot = moveHistory[moveHistoryLen - 4].loc.spot;
        Direction prev4dir = moveHistory[moveHistoryLen - 4].loc.dir;
        numTurnsOfHistoryIncluded = 1;
        if(prev4Spot != Board::NULL_LOC) {
          int pos = NNPos::spotToPos(prev4Spot, xSize, nnXLen, nnYLen);
          if(historyChannelWithDirection) {
            setRowBin(rowBin, pos, currentFeatureIdx + prev4dir, 1.0f, posStride, featureStride);
            currentFeatureIdx += 4;
          } else
            setRowBin(rowBin, pos, currentFeatureIdx++, 1.0f, posStride, featureStride);
        }
        if(numTurns >= 5 && moveHistory[moveHistoryLen - 5].pla == opp) {
          Spot prev5Spot = moveHistory[moveHistoryLen - 5].loc.spot;
          Direction prev5dir = moveHistory[moveHistoryLen - 5].loc.dir;
          numTurnsOfHistoryIncluded = 1;
          if(prev5Spot != Board::NULL_LOC) {
            int pos = NNPos::spotToPos(prev5Spot, xSize, nnXLen, nnYLen);
            if(historyChannelWithDirection) {
              setRowBin(rowBin, pos, currentFeatureIdx + prev5dir, 1.0f, posStride, featureStride);
              currentFeatureIdx += 4;
            } else
              setRowBin(rowBin, pos, currentFeatureIdx++, 1.0f, posStride, featureStride);
          }
        }
      }
    }
  }
  if(historyChannelWithDirection)
    assert(currentFeatureIdx == 20);
  else
    assert(currentFeatureIdx == 11);

  // Feature 11 or 20 - legal moves
  for(int x = 0; x < xSize; x++) {
    for(int y = 0; y < ySize; y++) {
      Spot spot = Location::getSpot(x, y, xSize);
      for(int dir = 0; dir < 4; dir++) {
        if(board.isLegal(Loc(spot, dir), pla)) {
          if(historyChannelWithDirection)
            setRowBin(rowBin, spot, currentFeatureIdx + dir, 1.0f, posStride, featureStride);
          else
            setRowBin(rowBin, spot, currentFeatureIdx + dir, 1.0f, posStride, featureStride);
        }
      }
    }
  }
  currentFeatureIdx += 4;
  // Feature 12~14 or 21~23 - consecutive stones
  for(int len = board.win_len - 1; len >= board.win_len - 3; --len) {
    int feature = currentFeatureIdx + board.win_len - 1 - len;
    board.fillRowWithLine(len, rowBin + feature * featureStride, nnXLen, nnYLen, posStride, featureStride);
  }

  // Global features.
  rowGlobal[0] = (float)board.win_len;
}
