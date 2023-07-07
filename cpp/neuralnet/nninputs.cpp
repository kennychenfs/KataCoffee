#include "../neuralnet/nninputs.h"

using namespace std;
bool NNInputs::historyChannelWithDirection = false;

// pPos is for policy output and it includes direction.
int NNPos::xydToPPos(int x, int y, Direction dir, int nnXLen, int nnYLen) {
  return (int)dir * nnXLen * nnYLen + (y * nnXLen + x);
}
int NNPos::actionToPPos(Action move, int boardXSize, int nnXLen, int nnYLen) {
  if(move.loc == Board::NULL_LOC)
    return nnXLen * nnYLen * NUM_DIRECTIONS;
  else
    return move.dir * nnXLen * nnYLen + (Location::getY(move.loc, boardXSize) * nnXLen + Location::getX(move.loc, boardXSize));//CHW format
}
Action NNPos::pPosToAction(int ppos, int boardXSize, int boardYSize, int nnXLen, int nnYLen) {
  int dir = ppos /(nnXLen*nnYLen);
  ppos %= (nnXLen*nnYLen);
  int x = ppos % nnXLen;
  int y = ppos / nnXLen;
  if(x < 0 || x >= boardXSize || y < 0 || y >= boardYSize)
    return Action(Board::NULL_LOC, D_NONE);
  return getAction(x, y, (Direction)dir, boardXSize);
}

//only for ownership map

int NNPos::xyToPos(int x, int y, int nnXLen) {
  return y * nnXLen + x;
}
int NNPos::locToPos(Loc loc, int boardXSize, int nnXLen, int nnYLen) {
  if(loc == Board::NULL_LOC)
    return nnXLen * nnYLen;
  return Location::getY(loc,boardXSize) * nnXLen + Location::getX(loc,boardXSize);
}
Loc NNPos::posToLoc(int pos, int boardXSize, int boardYSize, int nnXLen, int nnYLen) {
  int x = pos % nnXLen;
  int y = pos / nnXLen;
  if(x < 0 || x >= boardXSize || y < 0 || y >= boardYSize)
    return Board::NULL_LOC;
  return Location::getLoc(x,y,boardXSize);
}

int NNPos::getPolicySize(int nnXLen, int nnYLen) {
  return nnXLen * nnYLen * (NUM_DIRECTIONS - 1);//exclude D_NONE
}

//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------

const Hash128 MiscNNInputParams::ZOBRIST_PLAYOUT_DOUBLINGS =
  Hash128(0xa5e6114d380bfc1dULL, 0x4160557f1222f4adULL);
const Hash128 MiscNNInputParams::ZOBRIST_NN_POLICY_TEMP =
  Hash128(0xebcbdfeec6f4334bULL, 0xb85e43ee243b5ad2ULL);
const Hash128 MiscNNInputParams::ZOBRIST_POLICY_OPTIMISM =
  Hash128(0x88415c85c2801955ULL, 0x39bdf76b2aaa5eb1ULL);

//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------


double ScoreValue::whiteWinsOfWinner(Player winner) {
  if(winner == P_WHITE)
    return 1.0;
  else if(winner == P_BLACK)
    return 0.0;

  assert(false);
}

NNOutput::NNOutput()
  :whiteOwnerMap(NULL),noisedPolicyProbs(NULL) {}
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
  }
  else
    whiteOwnerMap = NULL;

  if(other.noisedPolicyProbs != NULL) {
    noisedPolicyProbs = new float[NNPos::MAX_NN_POLICY_SIZE];
    std::copy(other.noisedPolicyProbs, other.noisedPolicyProbs + NNPos::MAX_NN_POLICY_SIZE, noisedPolicyProbs);
  }
  else
    noisedPolicyProbs = NULL;

  std::copy(other.policyProbs, other.policyProbs+NNPos::MAX_NN_POLICY_SIZE, policyProbs);
}

NNOutput::NNOutput(const vector<shared_ptr<NNOutput>>& others) {
  assert(others.size() < 1000000);
  int len = (int)others.size();
  float floatLen = (float)len;
  assert(len > 0);
  for(int i = 1; i<len; i++) {
    assert(others[i]->nnHash == others[0]->nnHash);
  }
  nnHash = others[0]->nnHash;

  whiteWinProb = 0.0f;
  whiteLossProb = 0.0f;
  varTimeLeft = 0.0f;
  shorttermWinlossError = 0.0f;
  for(int i = 0; i<len; i++) {
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
    for(int i = 0; i<len; i++) {
      const NNOutput& other = *(others[i]);
      if(other.whiteOwnerMap != NULL) {
        if(whiteOwnerMap == NULL) {
          whiteOwnerMap = new float[nnXLen * nnYLen];
          std::fill(whiteOwnerMap, whiteOwnerMap + nnXLen * nnYLen, 0.0f);
        }
        whiteOwnerMapCount += 1.0f;
        for(int pos = 0; pos<nnXLen*nnYLen; pos++)
          whiteOwnerMap[pos] += other.whiteOwnerMap[pos];
      }
    }
    if(whiteOwnerMap != NULL) {
      assert(whiteOwnerMapCount > 0);
      for(int pos = 0; pos<nnXLen*nnYLen; pos++)
        whiteOwnerMap[pos] /= whiteOwnerMapCount;
    }
  }

  noisedPolicyProbs = NULL;

  //For technical correctness in case of impossibly rare hash collisions:
  //Just give up if they don't all match in move legality
  {
    bool mismatch = false;
    std::fill(policyProbs, policyProbs + NNPos::MAX_NN_POLICY_SIZE, 0.0f);
    for(int i = 0; i<len; i++) {
      const NNOutput& other = *(others[i]);
      for(int pos = 0; pos<NNPos::MAX_NN_POLICY_SIZE; pos++) {
        if(i > 0 && (policyProbs[pos] < 0) != (other.policyProbs[pos] < 0))// negative policy = illegal move
          mismatch = true;
        policyProbs[pos] += other.policyProbs[pos];
      }
    }
    //In case of illegal moves mismatch, just take the first one
    //This should basically never happen, only on true hash collisions
    if(mismatch) {
      const NNOutput& other = *(others[0]);
      std::copy(other.policyProbs, other.policyProbs + NNPos::MAX_NN_POLICY_SIZE, policyProbs);
    }
    else {
      for(int pos = 0; pos<NNPos::MAX_NN_POLICY_SIZE; pos++)
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
  }
  else
    whiteOwnerMap = NULL;
  if(noisedPolicyProbs != NULL)
    delete[] noisedPolicyProbs;
  if(other.noisedPolicyProbs != NULL) {
    noisedPolicyProbs = new float[NNPos::MAX_NN_POLICY_SIZE];
    std::copy(other.noisedPolicyProbs, other.noisedPolicyProbs + NNPos::MAX_NN_POLICY_SIZE, noisedPolicyProbs);
  }
  else
    noisedPolicyProbs = NULL;

  std::copy(other.policyProbs, other.policyProbs+NNPos::MAX_NN_POLICY_SIZE, policyProbs);

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
  out << "Win " << Global::strprintf("%.2fc",whiteWinProb*100) << endl;
  out << "Loss " << Global::strprintf("%.2fc",whiteLossProb*100) << endl;
  out << "VarTimeLeft " << Global::strprintf("%.1f",varTimeLeft) << endl;
  out << "STWinlossError " << Global::strprintf("%.2fc",shorttermWinlossError*100) << endl;

  out << "Policy" << endl;
  for(int y = 0; y<board.y_size; y++) {
    for(int x = 0; x<board.x_size; x++) {
      for(Direction dir = 0; dir < NUM_DIRECTIONS-1; ++dir) {
        int pos = NNPos::xydToPPos(x, y, dir, nnXLen, nnYLen);
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
    for(int y = 0; y<board.y_size; y++) {
      for(int x = 0; x<board.x_size; x++) {
        int pos = NNPos::xyToPos(x,y,nnXLen);
        float whiteOwn = whiteOwnerMap[pos];
        out << Global::strprintf("%5d ", (int)round(whiteOwn * 1000));
      }
      out << endl;
    }
    out << endl;
  }
}

//-------------------------------------------------------------------------------------------------------------

static void copyWithSymmetry(const float* src, float* dst, int nSize, int hSize, int wSize, int cSize, bool useNHWC, int symmetry, bool reverse) {
  bool transpose = (symmetry & 0x4) != 0 && hSize == wSize;
  bool flipX = (symmetry & 0x2) != 0;
  bool flipY = (symmetry & 0x1) != 0;
  if(transpose && !reverse)
    std::swap(flipX,flipY);
  if(useNHWC) {
    int nStride = hSize * wSize * cSize;
    int hStride = wSize * cSize;
    int wStride = cSize;
    int hBaseNew = 0; int hStrideNew = hStride;
    int wBaseNew = 0; int wStrideNew = wStride;

    if(flipY) { hBaseNew = (hSize-1) * hStrideNew; hStrideNew = -hStrideNew; }
    if(flipX) { wBaseNew = (wSize-1) * wStrideNew; wStrideNew = -wStrideNew; }

    if(transpose)
      std::swap(hStrideNew,wStrideNew);

    for(int n = 0; n<nSize; n++) {
      for(int h = 0; h<hSize; h++) {
        int nhOld = n * nStride + h*hStride;
        int nhNew = n * nStride + hBaseNew + h*hStrideNew;
        for(int w = 0; w<wSize; w++) {
          int nhwOld = nhOld + w*wStride;
          int nhwNew = nhNew + wBaseNew + w*wStrideNew;
          for(int c = 0; c<cSize; c++) {
            dst[nhwNew + c] = src[nhwOld + c];
          }
        }
      }
    }
  }
  else {
    int ncSize = nSize * cSize;
    int ncStride = hSize * wSize;
    int hStride = wSize;
    int wStride = 1;
    int hBaseNew = 0; int hStrideNew = hStride;
    int wBaseNew = 0; int wStrideNew = wStride;

    if(flipY) { hBaseNew = (hSize-1) * hStrideNew; hStrideNew = -hStrideNew; }
    if(flipX) { wBaseNew = (wSize-1) * wStrideNew; wStrideNew = -wStrideNew; }

    if(transpose)
      std::swap(hStrideNew,wStrideNew);

    for(int nc = 0; nc<ncSize; nc++) {
      for(int h = 0; h<hSize; h++) {
        int nchOld = nc * ncStride + h*hStride;
        int nchNew = nc * ncStride + hBaseNew + h*hStrideNew;
        for(int w = 0; w<wSize; w++) {
          int nchwOld = nchOld + w*wStride;
          int nchwNew = nchNew + wBaseNew + w*wStrideNew;
          dst[nchwNew] = src[nchwOld];
        }
      }
    }
  }
}


void SymmetryHelpers::copyInputsWithSymmetry(const float* src, float* dst, int nSize, int hSize, int wSize, int cSize, bool useNHWC, int symmetry) {
  copyWithSymmetry(src, dst, nSize, hSize, wSize, cSize, useNHWC, symmetry, false);
}

void SymmetryHelpers::copyOutputsWithSymmetry(const float* src, float* dst, int nSize, int hSize, int wSize, int symmetry) {
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
  return compose(compose(firstSymmetry,nextSymmetry),nextNextSymmetry);
}

Loc SymmetryHelpers::getSymLoc(int x, int y, int xSize, int ySize, int symmetry) {
  bool transpose = (symmetry & 0x4) != 0;
  bool flipX = (symmetry & 0x2) != 0;
  bool flipY = (symmetry & 0x1) != 0;
  if(flipX) { x = xSize - x - 1; }
  if(flipY) { y = ySize - y - 1; }

  if(transpose)
    std::swap(x,y);
  return Location::getLoc(x,y,transpose ? ySize : xSize);
}

Loc SymmetryHelpers::getSymLoc(int x, int y, const Board& board, int symmetry) {
  return getSymLoc(x,y,board.x_size,board.y_size,symmetry);
}

Loc SymmetryHelpers::getSymLoc(Loc loc, const Board& board, int symmetry) {
  if(loc == Board::NULL_LOC)
    return loc;
  return getSymLoc(Location::getX(loc,board.x_size), Location::getY(loc,board.x_size), board, symmetry);
}

Loc SymmetryHelpers::getSymLoc(Loc loc, int xSize, int ySize, int symmetry) {
  if(loc == Board::NULL_LOC)
    return loc;
  return getSymLoc(Location::getX(loc,xSize), Location::getY(loc,xSize), xSize, ySize, symmetry);
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
    case D_NORTHEAST: return D_NORTHWEST;
    case D_NORTHWEST: return D_NORTHEAST;
    }
  }
  if(isTranspose) {
    switch(dir) {
    case D_NORTH: return D_WEST;
    case D_WEST: return D_NORTH;
    }
  }
  assert(false);
}

Board SymmetryHelpers::getSymBoard(const Board& board, int symmetry) {
  bool transpose = (symmetry & 0x4) != 0;
  bool flipX = (symmetry & 0x2) != 0;
  bool flipY = (symmetry & 0x1) != 0;
  Board symBoard(
    transpose ? board.y_size : board.x_size,
    transpose ? board.x_size : board.y_size
  );
  Loc symKoLoc = Board::NULL_LOC;
  for(int y = 0; y<board.y_size; y++) {
    for(int x = 0; x<board.x_size; x++) {
      Loc loc = Location::getLoc(x,y,board.x_size);
      int symX = flipX ? board.x_size - x - 1 : x;
      int symY = flipY ? board.y_size - y - 1 : y;
      if(transpose)
        std::swap(symX,symY);
      Loc symLoc = Location::getLoc(symX,symY,symBoard.x_size);
      bool suc = symBoard.setStone(symLoc, board.colors[loc]);
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

//Currently does NOT depend on history (except for marking ko-illegal spots)
Hash128 NNInputs::getHash(
  const Board& board, const BoardHistory& hist, Player nextPlayer,
  const MiscNNInputParams& nnInputParams
) {
  Hash128 hash = board.getSitHash(nextPlayer);

  //Fold in whether the game is over or not, since this affects how we compute input features
  //but is not a function necessarily of previous hashed values.
  //If the history is in a weird prolonged state, also treat it similarly.
  if(hist.isGameFinished)
    hash ^= Board::ZOBRIST_GAME_IS_OVER;

  //Fold in asymmetric playout indicator
  if(nnInputParams.playoutDoublingAdvantage != 0) {
    int64_t playoutDoublingsDiscretized = (int64_t)(nnInputParams.playoutDoublingAdvantage*256.0f);
    hash.hash0 += Hash::splitMix64((uint64_t)playoutDoublingsDiscretized);
    hash.hash1 += Hash::basicLCong((uint64_t)playoutDoublingsDiscretized);
    hash ^= MiscNNInputParams::ZOBRIST_PLAYOUT_DOUBLINGS;
  }

  //Fold in policy temperature
  if(nnInputParams.nnPolicyTemperature != 1.0f) {
    int64_t nnPolicyTemperatureDiscretized = (int64_t)(nnInputParams.nnPolicyTemperature*2048.0f);
    hash.hash0 ^= Hash::basicLCong2((uint64_t)nnPolicyTemperatureDiscretized);
    hash.hash1 = Hash::splitMix64(hash.hash1 + (uint64_t)nnPolicyTemperatureDiscretized);
    hash.hash0 += hash.hash1;
    hash ^= MiscNNInputParams::ZOBRIST_NN_POLICY_TEMP;
  }

  //Fold in policy optimism
  if(nnInputParams.policyOptimism > 0) {
    hash ^= MiscNNInputParams::ZOBRIST_POLICY_OPTIMISM;
    int64_t policyOptimismDiscretized = (int64_t)(nnInputParams.policyOptimism*1024.0);
    hash.hash0 = Hash::rrmxmx(Hash::splitMix64(hash.hash0) + (uint64_t)policyOptimismDiscretized);
    hash.hash1 = Hash::rrmxmx(hash.hash1 + hash.hash0 + (uint64_t)policyOptimismDiscretized);
  }

  return hash;
}

//===========================================================================================
//INPUTSVERSION 1(edited V7 in KataGo)
//===========================================================================================

void NNInputs::fillRowV1(
  const Board& board, const BoardHistory& hist, Player nextPlayer,
  const MiscNNInputParams& nnInputParams,
  int nnXLen, int nnYLen, bool useNHWC, float* rowBin, float* rowGlobal
) {
  assert(nnXLen <= NNPos::MAX_BOARD_LEN);
  assert(nnYLen <= NNPos::MAX_BOARD_LEN);
  assert(board.x_size <= nnXLen);
  assert(board.y_size <= nnYLen);
  std::fill(rowBin,rowBin+NUM_FEATURES_SPATIAL_V1*nnXLen*nnYLen, 0.0f);
  std::fill(rowGlobal,rowGlobal+NUM_FEATURES_GLOBAL_V1, 0.0f);

  Player pla = nextPlayer;
  Player opp = getOpp(pla);
  int xSize = board.x_size;
  int ySize = board.y_size;

  int featureStride;
  int posStride;
  if(useNHWC) {
    featureStride = 1;
    posStride = NNInputs::NUM_FEATURES_SPATIAL_V1;
  }
  else {
    featureStride = nnXLen * nnYLen;
    posStride = 1;
  }

  for(int y = 0; y<ySize; y++) {
    for(int x = 0; x<xSize; x++) {
      int pos = NNPos::xyToPos(x,y,nnXLen);
      Loc loc = Location::getLoc(x,y,xSize);

      //Feature 0 - on board
      setRowBin(rowBin,pos,0, 1.0f, posStride, featureStride);

      Color stone = board.colors[loc];

      //Features 1,2 - pla,opp stone
      if(stone == pla)
        setRowBin(rowBin,pos,1, 1.0f, posStride, featureStride);
      else if(stone == opp)
        setRowBin(rowBin,pos,2, 1.0f, posStride, featureStride);
    }
  }

  //Feature 3~6 - last move.
  if(hist.moveHistory.size() > 0) {
    Loc loc = hist.moveHistory[hist.moveHistory.size()-1].loc;
    int pos = NNPos::locToPos(loc,board.x_size,nnXLen,nnYLen);
    Direction dir = hist.moveHistory[hist.moveHistory.size()-1].dir;
    setRowBin(rowBin,pos,3+dir, 1.0f, posStride, featureStride);
  }

  int numTurnsOfHistoryIncluded = 0;

  //Features 7~10 or 7~ - last 2,3,4,5 moves
  const vector<Move>& moveHistory = hist.moveHistory;
  size_t moveHistoryLen = moveHistory.size();
  //Also effectively wipe history as we change phase
  assert(moveHistoryLen >= hist.numTurns);
  int numTurns = hist.numTurns;

  if(numTurns >= 2 && moveHistory[moveHistoryLen-2].pla == pla) {
    Loc prev2Loc = moveHistory[moveHistoryLen-2].loc;
    Direction prev2dir = moveHistory[moveHistoryLen-2].dir;
    numTurnsOfHistoryIncluded = 1;
    if(prev2Loc != Board::NULL_LOC) {
      int pos = NNPos::locToPos(prev2Loc,xSize,nnXLen,nnYLen);
      if(historyChannelWithDirection)
        setRowBin(rowBin, pos, 7+prev2dir, 1.0f, posStride, featureStride);
      else
        setRowBin(rowBin, pos, 7, 1.0f, posStride, featureStride);
    }
    if(numTurns >= 3 && moveHistory[moveHistoryLen-3].pla == opp) {
      Loc prev3Loc = moveHistory[moveHistoryLen-3].loc;
      Direction prev3dir = moveHistory[moveHistoryLen-3].dir;
      numTurnsOfHistoryIncluded = 1;
      if(prev3Loc != Board::NULL_LOC) {
        int pos = NNPos::locToPos(prev3Loc,xSize,nnXLen,nnYLen);
        if(historyChannelWithDirection)
          setRowBin(rowBin, pos, 11+prev3dir, 1.0f, posStride, featureStride);
        else
          setRowBin(rowBin, pos, 8, 1.0f, posStride, featureStride);
      }
      if(numTurns >= 4 && moveHistory[moveHistoryLen-4].pla == pla) {
        Loc prev4Loc = moveHistory[moveHistoryLen-4].loc;
        Direction prev4dir = moveHistory[moveHistoryLen-4].dir;
        numTurnsOfHistoryIncluded = 1;
        if(prev4Loc != Board::NULL_LOC) {
          int pos = NNPos::locToPos(prev4Loc,xSize,nnXLen,nnYLen);
          if(historyChannelWithDirection)
            setRowBin(rowBin, pos, 15+prev4dir, 1.0f, posStride, featureStride);
          else
            setRowBin(rowBin, pos, 9, 1.0f, posStride, featureStride);
        }
        if(numTurns >= 5 && moveHistory[moveHistoryLen-5].pla == pla) {
          Loc prev5Loc = moveHistory[moveHistoryLen-5].loc;
          Direction prev5dir = moveHistory[moveHistoryLen-5].dir;
          numTurnsOfHistoryIncluded = 1;
          if(prev5Loc != Board::NULL_LOC) {
            int pos = NNPos::locToPos(prev5Loc,xSize,nnXLen,nnYLen);
            if(historyChannelWithDirection)
              setRowBin(rowBin, pos, 19+prev5dir, 1.0f, posStride, featureStride);
            else
              setRowBin(rowBin, pos, 10, 1.0f, posStride, featureStride);
          }
        }
      }
    }
  }

  //Feature 11 or 20 - legal moves
  for(int x = 0; x < xSize; x++) {
    for(int y = 0; y < ySize; y++) {
      Loc pos = Location::getLoc(x,y,xSize);
      for(int dir = 0; dir < 4; dir++) {
        if(board.isLegal(Action(pos, dir), pla)) {
          setRowBin(rowBin, pos, historyChannelWithDirection?20:11, 1.0f, posStride, featureStride);
        }
      }
    }
  }

  //Feature 12~14 or 21~23 - consecutive stones
  for(int len = board.win_len-1; len >= board.win_len-3; --len) {
    int feature = (historyChannelWithDirection?21:12)+board.win_len-1-len;
    board.fillRowWithLine(len, rowBin + feature * featureStride, nnXLen, nnYLen, posStride, featureStride);
  }

  //Global features.
  rowGlobal[0] = (float)board.win_len;
}
