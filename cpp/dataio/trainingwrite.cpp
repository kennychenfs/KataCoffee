#include "../dataio/trainingwrite.h"

#include "../core/fileutils.h"
#include "../neuralnet/modelversion.h"

using namespace std;

ValueTargets::ValueTargets() : win(0), loss(0) {}
ValueTargets::~ValueTargets() {}

//-------------------------------------------------------------------------------------

SidePosition::SidePosition()
  : board(),
    hist(),
    pla(P_BLACK),
    unreducedNumVisits(),
    policyTarget(),
    policySurprise(),
    policyEntropy(),
    searchEntropy(),
    whiteValueTargets(),
    targetWeight(),
    targetWeightUnrounded(),
    numNeuralNetChangesSoFar() {}

SidePosition::SidePosition(const Board& b, const BoardHistory& h, Player p, int numNNChangesSoFar)
  : board(b),
    hist(h),
    pla(p),
    unreducedNumVisits(),
    policyTarget(),
    policySurprise(),
    policyEntropy(),
    searchEntropy(),
    whiteValueTargets(),
    targetWeight(1.0f),
    targetWeightUnrounded(1.0f),
    numNeuralNetChangesSoFar(numNNChangesSoFar) {}

SidePosition::~SidePosition() {}

//-------------------------------------------------------------------------------------

FinishedGameData::FinishedGameData()
  : bName(),
    wName(),
    bIdx(0),
    wIdx(0),

    startBoard(),
    startHist(),
    endHist(),
    startPla(P_BLACK),
    gameHash(),

    playoutDoublingAdvantagePla(P_BLACK),
    playoutDoublingAdvantage(0.0),

    hitTurnLimit(false),

    numExtraBlack(0),
    mode(0),
    beganInEncorePhase(0),
    usedInitialPosition(0),

    hasFullData(false),
    targetWeightByTurn(),
    targetWeightByTurnUnrounded(),
    policyTargetsByTurn(),
    whiteValueTargetsByTurn(),
    nnRawStatsByTurn(),
    finalFullArea(NULL),
    finalOwnership(NULL),

    trainingWeight(1.0),

    sidePositions(),
    changedNeuralNets(),
    bTimeUsed(0.0),
    wTimeUsed(0.0),
    bMoveCount(0),
    wMoveCount(0) {}

FinishedGameData::~FinishedGameData() {
  for(size_t i = 0; i < policyTargetsByTurn.size(); i++)
    delete policyTargetsByTurn[i].policyTargets;

  if(finalFullArea != NULL)
    delete[] finalFullArea;
  if(finalOwnership != NULL)
    delete[] finalOwnership;

  for(size_t i = 0; i < sidePositions.size(); i++)
    delete sidePositions[i];

  for(size_t i = 0; i < changedNeuralNets.size(); i++)
    delete changedNeuralNets[i];
}

void FinishedGameData::printDebug(ostream& out) const {
  out << "bName " << bName << endl;
  out << "wName " << wName << endl;
  out << "bIdx " << bIdx << endl;
  out << "wIdx " << wIdx << endl;
  out << "startPla " << GameIO::colorToChar(startPla) << endl;
  out << "start" << endl;
  startHist.printDebugInfo(out, startBoard);
  out << "end" << endl;
  endHist.printDebugInfo(out, endHist.getRecentBoard(0));
  out << "gameHash " << gameHash << endl;
  out << "hitTurnLimit " << hitTurnLimit << endl;
  out << "numExtraBlack " << numExtraBlack << endl;
  out << "mode " << mode << endl;
  out << "beganInEncorePhase " << beganInEncorePhase << endl;
  out << "usedInitialPosition " << usedInitialPosition << endl;
  out << "hasFullData " << hasFullData << endl;
  for(int i = 0; i < targetWeightByTurn.size(); i++)
    out << "targetWeightByTurn " << i << " " << targetWeightByTurn[i] << " "
        << "unrounded"
        << " " << targetWeightByTurnUnrounded[i] << endl;
  for(int i = 0; i < policyTargetsByTurn.size(); i++) {
    out << "policyTargetsByTurn " << i << " ";
    out << "unreducedNumVisits " << policyTargetsByTurn[i].unreducedNumVisits << " ";
    if(policyTargetsByTurn[i].policyTargets != NULL) {
      const vector<PolicyTargetMove>& target = *(policyTargetsByTurn[i].policyTargets);
      for(int j = 0; j < target.size(); j++)
        out << GameIO::locToString(target[j].loc, startBoard) << " " << target[j].policyTarget << " ";
    }
    out << endl;
  }
  for(int i = 0; i < policySurpriseByTurn.size(); i++)
    out << "policySurpriseByTurn " << i << " " << policySurpriseByTurn[i] << endl;
  for(int i = 0; i < policyEntropyByTurn.size(); i++)
    out << "policyEntropyByTurn " << i << " " << policyEntropyByTurn[i] << endl;
  for(int i = 0; i < searchEntropyByTurn.size(); i++)
    out << "searchEntropyByTurn " << i << " " << searchEntropyByTurn[i] << endl;

  for(int i = 0; i < whiteValueTargetsByTurn.size(); i++) {
    out << "whiteValueTargetsByTurn " << i << " ";
    out << whiteValueTargetsByTurn[i].win << " ";
    out << whiteValueTargetsByTurn[i].loss << " ";
    out << endl;
  }
  for(int i = 0; i < nnRawStatsByTurn.size(); i++) {
    out << "Raw Stats " << nnRawStatsByTurn[i].whiteWinLoss << " " << nnRawStatsByTurn[i].policyEntropy << endl;
  }
  if(finalFullArea != NULL) {
    for(int y = 0; y < startBoard.y_size; y++) {
      for(int x = 0; x < startBoard.x_size; x++) {
        Spot spot = Location::getSpot(x, y, startBoard.x_size);
        out << GameIO::colorToChar(finalFullArea[spot]);
      }
      out << endl;
    }
  }
  if(finalOwnership != NULL) {
    for(int y = 0; y < startBoard.y_size; y++) {
      for(int x = 0; x < startBoard.x_size; x++) {
        Spot spot = Location::getSpot(x, y, startBoard.x_size);
        out << GameIO::colorToChar(finalOwnership[spot]);
      }
      out << endl;
    }
  }
  out << "trainingWeight " << trainingWeight << endl;
  for(int i = 0; i < sidePositions.size(); i++) {
    SidePosition* sp = sidePositions[i];
    out << "Side position " << i << endl;
    out << "targetWeight " << sp->targetWeight << " "
        << "unrounded"
        << " " << sp->targetWeightUnrounded << endl;
    sp->hist.printDebugInfo(out, sp->board);
  }
}

//-------------------------------------------------------------------------------------

// Don't forget to update everything else in the header file and the code below too if changing any of these
// And update the python code
static const int POLICY_TARGET_NUM_CHANNELS = 2;
static const int GLOBAL_TARGET_NUM_CHANNELS = 64;
static const int VALUE_SPATIAL_TARGET_NUM_CHANNELS = 5;

TrainingWriteBuffers::TrainingWriteBuffers(
  int iVersion,
  int maxRws,
  int numBChannels,
  int numFChannels,
  int xLen,
  int yLen)
  : inputsVersion(iVersion),
    maxRows(maxRws),
    numBinaryChannels(numBChannels),
    numGlobalChannels(numFChannels),
    dataXLen(xLen),
    dataYLen(yLen),
    packedBoardArea((xLen * yLen + 7) / 8),
    curRows(0),
    binaryInputNCHWUnpacked(NULL),
    binaryInputNCHWPacked({maxRws, numBChannels, packedBoardArea}),
    globalInputNC({maxRws, numFChannels}),
    policyTargetsNCMove({maxRws, POLICY_TARGET_NUM_CHANNELS, NNPos::getPolicySize(xLen, yLen)}),
    globalTargetsNC({maxRws, GLOBAL_TARGET_NUM_CHANNELS}),
    valueTargetsNCHW({maxRws, VALUE_SPATIAL_TARGET_NUM_CHANNELS, yLen, xLen}) {
  binaryInputNCHWUnpacked = new float[numBChannels * xLen * yLen];
}

TrainingWriteBuffers::~TrainingWriteBuffers() {
  delete[] binaryInputNCHWUnpacked;
}

void TrainingWriteBuffers::clear() {
  curRows = 0;
}

// Copy floats that are all 0-1 into bits, packing 8 to a byte, big-endian-style within each byte.
static void packBits(const float* binaryFloats, int len, uint8_t* bits) {
  for(int i = 0; i < len; i += 8) {
    if(i + 8 <= len) {
      bits[i >> 3] = ((uint8_t)binaryFloats[i + 0] << 7) | ((uint8_t)binaryFloats[i + 1] << 6) |
                     ((uint8_t)binaryFloats[i + 2] << 5) | ((uint8_t)binaryFloats[i + 3] << 4) |
                     ((uint8_t)binaryFloats[i + 4] << 3) | ((uint8_t)binaryFloats[i + 5] << 2) |
                     ((uint8_t)binaryFloats[i + 6] << 1) | ((uint8_t)binaryFloats[i + 7] << 0);
    } else {
      bits[i >> 3] = 0;
      for(int di = 0; i + di < len; di++) {
        bits[i >> 3] |= ((uint8_t)binaryFloats[i + di] << (7 - di));
      }
    }
  }
}

static void zeroPolicyTarget(int policySize, int16_t* target) {
  for(int pos = 0; pos < policySize; pos++)
    target[pos] = 0;
}

static void uniformPolicyTarget(int policySize, int16_t* target) {
  for(int pos = 0; pos < policySize; pos++)
    target[pos] = 1;
}

// Copy playouts into target, expanding out the sparse representation into a full plane.
static void fillPolicyTarget(
  const vector<PolicyTargetMove>& policyTargetMoves,
  int policySize,
  int dataXLen,
  int dataYLen,
  int boardXSize,
  int16_t* target) {
  zeroPolicyTarget(policySize, target);
  size_t size = policyTargetMoves.size();
  for(size_t i = 0; i < size; i++) {
    const PolicyTargetMove& move = policyTargetMoves[i];
    int pos = NNPos::locToPos(move.loc, boardXSize, dataXLen, dataYLen);
    assert(pos >= 0 && pos < policySize);
    target[pos] = move.policyTarget;
  }
}

// Converts a value in [-1,1] to an integer in [-120,120] to pack down to 8 bits.
// Randomizes to make sure the expectation is exactly correct.
static int8_t convertRadiusOneToRadius120(float x, Rand& rand) {
  // We need to pack this down to 8 bits, so map into [-120,120].
  // Randomize to ensure the expectation is exactly correct.
  x *= 120.0f;
  int low = (int)floor(x);
  int high = low + 1;
  if(low < -120)
    return -120;
  if(high > 120)
    return 120;

  float lambda = (float)(x - low);
  if(lambda == 0.0f)
    return (int8_t)low;
  else
    return (int8_t)(rand.nextBool(lambda) ? high : low);
}
// Same, but for [-2,2].
//  static int8_t convertRadiusTwoToRadius120(float x, Rand& rand) {
//    return convertRadiusOneToRadius120(x*0.5,rand);
//  }

static void fillValueTDTargets(
  const vector<ValueTargets>& whiteValueTargetsByTurn,
  int idx,
  Player nextPlayer,
  double nowFactor,
  float* buf) {
  double winValue = 0.0;
  double lossValue = 0.0;

  double weightLeft = 1.0;
  for(int i = idx; i < whiteValueTargetsByTurn.size(); i++) {
    double weightNow;
    if(i == whiteValueTargetsByTurn.size() - 1) {
      weightNow = weightLeft;
      weightLeft = 0.0;
    } else {
      weightNow = weightLeft * nowFactor;
      weightLeft *= (1.0 - nowFactor);
    }

    // Training rows need things from the perspective of the player to move, so we flip as appropriate.
    const ValueTargets& targets = whiteValueTargetsByTurn[i];
    winValue += weightNow * (nextPlayer == P_WHITE ? targets.win : targets.loss);
    lossValue += weightNow * (nextPlayer == P_WHITE ? targets.loss : targets.win);
  }

  buf[0] = (float)winValue;
  buf[1] = (float)lossValue;
}

void TrainingWriteBuffers::addRow(
  const Board& board,
  const BoardHistory& hist,
  Player nextPlayer,
  int turnIdx,
  float targetWeight,
  int64_t unreducedNumVisits,
  const vector<PolicyTargetMove>* policyTarget0,  // can be null
  const vector<PolicyTargetMove>* policyTarget1,  // can be null
  double policySurprise,
  double policyEntropy,
  double searchEntropy,
  const vector<ValueTargets>& whiteValueTargets,
  int whiteValueTargetsIdx,  // index in whiteValueTargets corresponding to this turn.
  const NNRawStats& nnRawStats,
  const Board* finalBoard,
  Color* finalOwnership,
  int* finalMaxLength,
  const vector<Board>* posHistForFutureBoards,  // can be null
  bool isSidePosition,
  int numNeuralNetsBehindLatest,
  const FinishedGameData& data,
  Rand& rand) {
  static_assert(NNModelVersion::latestInputsVersionImplemented == 1, "");
  if(
    inputsVersion < NNModelVersion::oldestInputsVersionImplemented ||
    inputsVersion > NNModelVersion::latestInputsVersionImplemented)
    throw StringError("Training write buffers: Does not support input version: " + Global::intToString(inputsVersion));

  int posArea = dataXLen * dataYLen;
  assert(data.hasFullData);
  assert(curRows < maxRows);

  {
    MiscNNInputParams nnInputParams;
    // Note: this is coordinated with the fact that selfplay does not use this feature on side positions
    if(!isSidePosition)
      nnInputParams.playoutDoublingAdvantage = getOpp(nextPlayer) == data.playoutDoublingAdvantagePla
                                                 ? -data.playoutDoublingAdvantage
                                                 : data.playoutDoublingAdvantage;

    bool inputsUseNHWC = false;
    float* rowBin = binaryInputNCHWUnpacked;
    float* rowGlobal = globalInputNC.data + curRows * numGlobalChannels;
    static_assert(NNModelVersion::latestInputsVersionImplemented == 1, "");
    if(inputsVersion == 1) {
      assert(NNInputs::NUM_FEATURES_SPATIAL_V1 == numBinaryChannels);
      assert(NNInputs::NUM_FEATURES_GLOBAL_V1 == numGlobalChannels);
      NNInputs::fillRowV1(board, hist, nextPlayer, nnInputParams, dataXLen, dataYLen, inputsUseNHWC, rowBin, rowGlobal);
    } else
      ASSERT_UNREACHABLE;

    // Pack bools bitwise into uint8_t
    uint8_t* rowBinPacked = binaryInputNCHWPacked.data + curRows * numBinaryChannels * packedBoardArea;
    for(int c = 0; c < numBinaryChannels; c++)
      packBits(rowBin + c * posArea, posArea, rowBinPacked + c * packedBoardArea);
  }

  // Vector for global targets and metadata
  float* rowGlobal = globalTargetsNC.data + curRows * GLOBAL_TARGET_NUM_CHANNELS;

  // Target weight for the whole row
  rowGlobal[25] = targetWeight;

  // Fill policy
  int policySize = NNPos::getPolicySize(dataXLen, dataYLen);
  int16_t* rowPolicy = policyTargetsNCMove.data + curRows * POLICY_TARGET_NUM_CHANNELS * policySize;

  if(policyTarget0 != NULL) {
    fillPolicyTarget(*policyTarget0, policySize, dataXLen, dataYLen, board.x_size, rowPolicy + 0 * policySize);
    rowGlobal[26] = 1.0f;
  } else {
    uniformPolicyTarget(policySize, rowPolicy + 0 * policySize);
    rowGlobal[26] = 0.0f;
  }

  if(policyTarget1 != NULL) {
    fillPolicyTarget(*policyTarget1, policySize, dataXLen, dataYLen, board.x_size, rowPolicy + 1 * policySize);
    rowGlobal[28] = 1.0f;
  } else {
    uniformPolicyTarget(policySize, rowPolicy + 1 * policySize);
    rowGlobal[28] = 0.0f;
  }

  // Fill td-like value targets
  int boardArea = board.x_size * board.y_size;
  assert(whiteValueTargetsIdx >= 0 && whiteValueTargetsIdx < whiteValueTargets.size());
  fillValueTDTargets(whiteValueTargets, whiteValueTargetsIdx, nextPlayer, 0.0, rowGlobal);
  // These three constants used to be 'nicer' numbers 0.18, 0.06, 0.02, but we screwed up the functional form
  // by omitting the "1.0 +" at the front (breaks scaling to small board sizes), so when we fixed this we also
  // decreased the other numbers slightly to try to maximally limit the impact of the fix on the numerical values
  // on the actual board sizes 9-19, since it would be costly to retest.
  fillValueTDTargets(
    whiteValueTargets, whiteValueTargetsIdx, nextPlayer, 1.0 / (1.0 + boardArea * 0.176), rowGlobal + 2);
  fillValueTDTargets(
    whiteValueTargets, whiteValueTargetsIdx, nextPlayer, 1.0 / (1.0 + boardArea * 0.056), rowGlobal + 4);
  fillValueTDTargets(
    whiteValueTargets, whiteValueTargetsIdx, nextPlayer, 1.0 / (1.0 + boardArea * 0.016), rowGlobal + 6);
  fillValueTDTargets(whiteValueTargets, whiteValueTargetsIdx, nextPlayer, 1.0, rowGlobal + 8);

  // Expected time of arrival of winloss variance, in turns
  {
    double sum = 0.0;
    for(int i = whiteValueTargetsIdx + 1; i < whiteValueTargets.size(); i++) {
      int turnsFromNow = i - whiteValueTargetsIdx;
      const ValueTargets& prevTargets = whiteValueTargets[i - 1];
      const ValueTargets& targets = whiteValueTargets[i];
      double prevWL = prevTargets.win - prevTargets.loss;
      double nextWL = targets.win - targets.loss;
      double variance = (nextWL - prevWL) * (nextWL - prevWL);
      sum += turnsFromNow * variance;
    }
    rowGlobal[22] = (float)sum;
  }

  // Reserved for future use
  rowGlobal[23] = 0.0f;
  rowGlobal[24] = 0.0f;
  rowGlobal[30] = (float)policySurprise;
  rowGlobal[31] = (float)policyEntropy;
  rowGlobal[32] = (float)searchEntropy;
  rowGlobal[35] = 0.0f;
  // Fill in whether we should use history or not
  bool useHist0 = rand.nextDouble() < 0.98;
  bool useHist1 = useHist0 && rand.nextDouble() < 0.98;
  bool useHist2 = useHist1 && rand.nextDouble() < 0.98;
  bool useHist3 = useHist2 && rand.nextDouble() < 0.98;
  bool useHist4 = useHist3 && rand.nextDouble() < 0.98;
  rowGlobal[36] = useHist0 ? 1.0f : 0.0f;
  rowGlobal[37] = useHist1 ? 1.0f : 0.0f;
  rowGlobal[38] = useHist2 ? 1.0f : 0.0f;
  rowGlobal[39] = useHist3 ? 1.0f : 0.0f;
  rowGlobal[40] = useHist4 ? 1.0f : 0.0f;

  // Fill in hash of game
  Hash128 gameHash = data.gameHash;
  rowGlobal[41] = (float)(gameHash.hash0 & 0x3FFFFF);  // 0x3FFFFF=2^22-1 covers 22 bits
  rowGlobal[42] = (float)((gameHash.hash0 >> 22) & 0x3FFFFF);
  rowGlobal[43] = (float)((gameHash.hash0 >> 44) & 0xFFFFF);
  rowGlobal[44] = (float)(gameHash.hash1 & 0x3FFFFF);
  rowGlobal[45] = (float)((gameHash.hash1 >> 22) & 0x3FFFFF);
  rowGlobal[46] = (float)((gameHash.hash1 >> 44) & 0xFFFFF);

  // Earlier neural net metadata
  rowGlobal[49] = data.changedNeuralNets.size() > 0 ? 1.0f : 0.0f;
  rowGlobal[50] = (float)numNeuralNetsBehindLatest;

  // Some misc metadata
  rowGlobal[51] = (float)turnIdx;
  rowGlobal[53] = (float)data.startHist.moveHistory.size();

  // Metadata about how the game was initialized
  rowGlobal[55] = (float)data.mode;
  rowGlobal[56] = (float)hist.initialTurnNumber;

  // Some stats
  rowGlobal[57] = (float)(nextPlayer == P_WHITE ? nnRawStats.whiteWinLoss : -nnRawStats.whiteWinLoss);
  rowGlobal[59] = (float)nnRawStats.policyEntropy;

  // Original number of visits
  rowGlobal[60] = (float)unreducedNumVisits;

  // Unused
  rowGlobal[62] = 0.0f;

  // Version
  rowGlobal[63] = 1.0f;

  assert(64 == GLOBAL_TARGET_NUM_CHANNELS);

  int8_t* rowOwnership = valueTargetsNCHW.data + curRows * VALUE_SPATIAL_TARGET_NUM_CHANNELS * posArea;

  if(finalOwnership == NULL) {
    rowGlobal[27] = 0.0f;
    for(int i = 0; i < posArea * 2; i++)
      rowOwnership[i] = 0;
  } else {
    assert(finalBoard != NULL);

    rowGlobal[27] = 1.0f;
    // Fill score info
    const ValueTargets& lastTargets = whiteValueTargets[whiteValueTargets.size() - 1];

    // Fill with zeros in case the buffers differ in size
    for(int i = 0; i < posArea * 2; i++)
      rowOwnership[i] = 0;

    // Fill ownership info
    Player opp = getOpp(nextPlayer);
    for(int y = 0; y < board.y_size; y++) {
      for(int x = 0; x < board.x_size; x++) {
        int pos = NNPos::xyToPos(x, y, dataXLen);
        Spot spot = Location::getSpot(x, y, board.x_size);
        if(finalOwnership[spot] == nextPlayer)
          rowOwnership[pos] = 1;
        else if(finalOwnership[spot] == opp)
          rowOwnership[pos] = -1;
      }
    }
  }

  if(posHistForFutureBoards == NULL) {
    rowGlobal[33] = 0.0f;
    for(int i = 0; i < posArea; i++) {
      rowOwnership[i + posArea * 2] = 0;
      rowOwnership[i + posArea * 3] = 0;
    }
  } else {
    const vector<Board>& boards = *posHistForFutureBoards;
    assert(boards.size() == whiteValueTargets.size());
    assert(boards.size() > 0);

    rowGlobal[33] = 1.0f;
    int endIdx = (int)boards.size() - 1;
    // 2 and 6 are set to let NN see farther into the future. They are 8 and 32 in KataGo, but Coffee has fewer moves.
    const Board& board2 = boards[std::min(whiteValueTargetsIdx + 2, endIdx)];
    const Board& board3 = boards[std::min(whiteValueTargetsIdx + 6, endIdx)];
    assert(board2.y_size == board.y_size && board2.x_size == board.x_size);
    assert(board3.y_size == board.y_size && board3.x_size == board.x_size);

    for(int i = 0; i < posArea; i++) {
      rowOwnership[i + posArea * 2] = 0;
      rowOwnership[i + posArea * 3] = 0;
    }
    Player pla = nextPlayer;
    Player opp = getOpp(nextPlayer);
    for(int y = 0; y < board.y_size; y++) {
      for(int x = 0; x < board.x_size; x++) {
        int pos = NNPos::xyToPos(x, y, dataXLen);
        Spot spot = Location::getSpot(x, y, board.x_size);
        if(board2.colors[spot] == pla)
          rowOwnership[pos + posArea * 2] = 1;
        else if(board2.colors[spot] == opp)
          rowOwnership[pos + posArea * 2] = -1;
        if(board3.colors[spot] == pla)
          rowOwnership[pos + posArea * 3] = 1;
        else if(board3.colors[spot] == opp)
          rowOwnership[pos + posArea * 3] = -1;
      }
    }
  }
  for(int y = 0; y < board.y_size; y++) {
    for(int x = 0; x < board.x_size; x++) {
      int pos = NNPos::xyToPos(x, y, dataXLen);
      Spot spot = Location::getSpot(x, y, board.x_size);
      rowOwnership[pos + posArea * 4] = finalMaxLength[spot];
    }
  }
  curRows++;
}
void TrainingWriteBuffers::writeToZipFile(const string& fileName) {
  ZipFile zipFile(fileName);

  uint64_t numBytes;

  numBytes = binaryInputNCHWPacked.prepareHeaderWithNumRows(curRows);
  zipFile.writeBuffer("binaryInputNCHWPacked", binaryInputNCHWPacked.dataIncludingHeader, numBytes);

  numBytes = globalInputNC.prepareHeaderWithNumRows(curRows);
  zipFile.writeBuffer("globalInputNC", globalInputNC.dataIncludingHeader, numBytes);

  numBytes = policyTargetsNCMove.prepareHeaderWithNumRows(curRows);
  zipFile.writeBuffer("policyTargetsNCMove", policyTargetsNCMove.dataIncludingHeader, numBytes);

  numBytes = globalTargetsNC.prepareHeaderWithNumRows(curRows);
  zipFile.writeBuffer("globalTargetsNC", globalTargetsNC.dataIncludingHeader, numBytes);

  numBytes = valueTargetsNCHW.prepareHeaderWithNumRows(curRows);
  zipFile.writeBuffer("valueTargetsNCHW", valueTargetsNCHW.dataIncludingHeader, numBytes);

  zipFile.close();
}

void TrainingWriteBuffers::writeToTextOstream(ostream& out) {
  int64_t len;

  auto printHeader = [&out](const char* dataIncludingHeader) {
    // In actuality our headers aren't that long, so we cut it off at half the total header bytes
    for(int i = 0; i < 10; i++)
      out << (int)dataIncludingHeader[i] << " ";
    for(int i = 10; i < NumpyBuffer<int>::TOTAL_HEADER_BYTES / 2; i++)
      out << dataIncludingHeader[i];
    out << endl;
  };

  out << "binaryInputNCHWPacked" << endl;
  binaryInputNCHWPacked.prepareHeaderWithNumRows(curRows);
  char buf[32];
  printHeader((const char*)binaryInputNCHWPacked.dataIncludingHeader);
  len = binaryInputNCHWPacked.getActualDataLen(curRows);
  for(int i = 0; i < len; i++) {
    sprintf(buf, "%02X", binaryInputNCHWPacked.data[i]);
    out << buf;
    if((i + 1) % (len / curRows) == 0)
      out << endl;
  }
  out << endl;

  out << "globalInputNC" << endl;
  globalInputNC.prepareHeaderWithNumRows(curRows);
  printHeader((const char*)globalInputNC.dataIncludingHeader);
  len = globalInputNC.getActualDataLen(curRows);
  for(int i = 0; i < len; i++) {
    out << globalInputNC.data[i] << " ";
    if((i + 1) % (len / curRows) == 0)
      out << endl;
  }
  out << endl;

  out << "policyTargetsNCMove" << endl;
  policyTargetsNCMove.prepareHeaderWithNumRows(curRows);
  printHeader((const char*)policyTargetsNCMove.dataIncludingHeader);
  len = policyTargetsNCMove.getActualDataLen(curRows);
  for(int i = 0; i < len; i++) {
    out << policyTargetsNCMove.data[i] << " ";
    if((i + 1) % (len / curRows) == 0)
      out << endl;
  }
  out << endl;

  out << "globalTargetsNC" << endl;
  globalTargetsNC.prepareHeaderWithNumRows(curRows);
  printHeader((const char*)globalTargetsNC.dataIncludingHeader);
  len = globalTargetsNC.getActualDataLen(curRows);
  for(int i = 0; i < len; i++) {
    out << globalTargetsNC.data[i] << " ";
    if((i + 1) % (len / curRows) == 0)
      out << endl;
  }
  out << endl;

  out << "valueTargetsNCHW" << endl;
  valueTargetsNCHW.prepareHeaderWithNumRows(curRows);
  printHeader((const char*)valueTargetsNCHW.dataIncludingHeader);
  len = valueTargetsNCHW.getActualDataLen(curRows);
  for(int i = 0; i < len; i++) {
    out << (int)valueTargetsNCHW.data[i] << " ";
    if((i + 1) % (len / curRows) == 0)
      out << endl;
  }
  out << endl;
}

//-------------------------------------------------------------------------------------

TrainingDataWriter::TrainingDataWriter(
  const string& outDir,
  int iVersion,
  int maxRowsPerFile,
  double firstFileMinRandProp,
  int dataXLen,
  int dataYLen,
  const string& randSeed)
  : TrainingDataWriter(outDir, NULL, iVersion, maxRowsPerFile, firstFileMinRandProp, dataXLen, dataYLen, 1, randSeed) {}
TrainingDataWriter::TrainingDataWriter(
  ostream* dbgOut,
  int iVersion,
  int maxRowsPerFile,
  double firstFileMinRandProp,
  int dataXLen,
  int dataYLen,
  int onlyEvery,
  const string& randSeed)
  : TrainingDataWriter(
      string(),
      dbgOut,
      iVersion,
      maxRowsPerFile,
      firstFileMinRandProp,
      dataXLen,
      dataYLen,
      onlyEvery,
      randSeed) {}

TrainingDataWriter::TrainingDataWriter(
  const string& outDir,
  ostream* dbgOut,
  int iVersion,
  int maxRowsPerFile,
  double firstFileMinRandProp,
  int dataXLen,
  int dataYLen,
  int onlyEvery,
  const string& randSeed)
  : outputDir(outDir),
    inputsVersion(iVersion),
    rand(randSeed),
    writeBuffers(NULL),
    debugOut(dbgOut),
    debugOnlyWriteEvery(onlyEvery),
    rowCount(0) {
  int numBinaryChannels;
  int numGlobalChannels;
  // Note that this inputsVersion is for data writing, it might be different than the inputsVersion used
  // to feed into a model during selfplay
  static_assert(NNModelVersion::latestInputsVersionImplemented == 1, "");
  if(inputsVersion == 1) {
    numBinaryChannels = NNInputs::NUM_FEATURES_SPATIAL_V1;
    numGlobalChannels = NNInputs::NUM_FEATURES_GLOBAL_V1;
  } else {
    throw StringError("TrainingDataWriter: Unsupported inputs version: " + Global::intToString(inputsVersion));
  }

  writeBuffers =
    new TrainingWriteBuffers(inputsVersion, maxRowsPerFile, numBinaryChannels, numGlobalChannels, dataXLen, dataYLen);

  if(firstFileMinRandProp < 0 || firstFileMinRandProp > 1)
    throw StringError(
      "TrainingDataWriter: firstFileMinRandProp not in [0,1]: " + Global::doubleToString(firstFileMinRandProp));
  isFirstFile = true;
  if(firstFileMinRandProp >= 1.0)
    firstFileMaxRows = maxRowsPerFile;
  else
    firstFileMaxRows = maxRowsPerFile - (int)(maxRowsPerFile * (1.0 - firstFileMinRandProp) * rand.nextDouble());
}

TrainingDataWriter::~TrainingDataWriter() {
  delete writeBuffers;
}

bool TrainingDataWriter::isEmpty() const {
  return writeBuffers->curRows <= 0;
}
int64_t TrainingDataWriter::numRowsInBuffer() const {
  return writeBuffers->curRows;
}

void TrainingDataWriter::writeAndClearIfFull() {
  if(writeBuffers->curRows >= writeBuffers->maxRows || (isFirstFile && writeBuffers->curRows >= firstFileMaxRows)) {
    flushIfNonempty();
  }
}

void TrainingDataWriter::flushIfNonempty() {
  string resultingFilename;
  flushIfNonempty(resultingFilename);
}

bool TrainingDataWriter::flushIfNonempty(string& resultingFilename) {
  if(writeBuffers->curRows <= 0)
    return false;

  isFirstFile = false;

  if(debugOut != NULL) {
    writeBuffers->writeToTextOstream(*debugOut);
    writeBuffers->clear();
    resultingFilename = "";
  } else {
    resultingFilename = outputDir + "/" + Global::uint64ToHexString(rand.nextUInt64()) + ".npz";
    string tmpFilename = resultingFilename + ".tmp";
    writeBuffers->writeToZipFile(tmpFilename);
    writeBuffers->clear();
    FileUtils::rename(tmpFilename, resultingFilename);
  }
  return true;
}

void TrainingDataWriter::writeGame(const FinishedGameData& data) {
  int numMoves = (int)(data.endHist.moveHistory.size() - data.startHist.moveHistory.size());
  assert(numMoves >= 0);
  assert(data.startHist.moveHistory.size() <= data.endHist.moveHistory.size());
  assert(data.endHist.moveHistory.size() <= 100000000);
  assert(data.targetWeightByTurn.size() == numMoves);
  assert(data.targetWeightByTurnUnrounded.size() == numMoves);
  assert(data.policyTargetsByTurn.size() == numMoves);
  assert(data.policySurpriseByTurn.size() == numMoves);
  assert(data.policyEntropyByTurn.size() == numMoves);
  assert(data.searchEntropyByTurn.size() == numMoves);
  assert(data.whiteValueTargetsByTurn.size() == numMoves + 1);
  assert(data.nnRawStatsByTurn.size() == numMoves);

// Some sanity checks
#ifndef NDEBUG
  {
    const ValueTargets& lastTargets = data.whiteValueTargetsByTurn[data.whiteValueTargetsByTurn.size() - 1];
    if(!data.endHist.isGameFinished)
      assert(data.hitTurnLimit);
    else if(data.endHist.winner == P_BLACK)
      assert(lastTargets.win == 0.0f && lastTargets.loss == 1.0f);
    else if(data.endHist.winner == P_WHITE)
      assert(lastTargets.win == 1.0f && lastTargets.loss == 0.0f);

    assert(data.finalFullArea != NULL);
    assert(data.finalOwnership != NULL);
    assert(data.finalMaxLength != NULL);
    assert(!data.endHist.isResignation);
  }
#endif

  // Play out all the moves in a single pass first to compute all the future board states
  vector<Board> posHistForFutureBoards;
  {
    Board board(data.startBoard);
    BoardHistory hist(data.startHist);
    Player nextPlayer = data.startPla;
    posHistForFutureBoards.push_back(board);

    int startTurnIdx = (int)data.startHist.moveHistory.size();
    for(int turnAfterStart = 0; turnAfterStart < numMoves; turnAfterStart++) {
      int turnIdx = turnAfterStart + startTurnIdx;

      Move move = data.endHist.moveHistory[turnIdx];
      assert(move.pla == nextPlayer);
      assert(hist.isLegal(board, move.loc, move.pla));
      hist.makeBoardMoveAssumeLegal(board, move.loc, move.pla);
      nextPlayer = getOpp(nextPlayer);

      posHistForFutureBoards.push_back(board);
    }
  }

  Board board(data.startBoard);
  BoardHistory hist(data.startHist);
  Player nextPlayer = data.startPla;

  // Write main game rows
  int startTurnIdx = (int)data.startHist.moveHistory.size();
  for(int turnAfterStart = 0; turnAfterStart < numMoves; turnAfterStart++) {
    double targetWeight = data.targetWeightByTurn[turnAfterStart];
    int turnIdx = turnAfterStart + startTurnIdx;

    int64_t unreducedNumVisits = data.policyTargetsByTurn[turnAfterStart].unreducedNumVisits;
    const vector<PolicyTargetMove>* policyTarget0 = data.policyTargetsByTurn[turnAfterStart].policyTargets;
    const vector<PolicyTargetMove>* policyTarget1 =
      (turnAfterStart + 1 < numMoves) ? data.policyTargetsByTurn[turnAfterStart + 1].policyTargets : NULL;
    bool isSidePosition = false;

    int numNeuralNetsBehindLatest = 0;
    for(int i = 0; i < data.changedNeuralNets.size(); i++) {
      if(data.changedNeuralNets[i]->turnIdx > turnIdx) {
        numNeuralNetsBehindLatest = (int)data.changedNeuralNets.size() - i;
        break;
      }
    }

    while(targetWeight > 0.0) {
      if(targetWeight >= 1.0 || rand.nextBool(targetWeight)) {
        if(debugOut == NULL || rowCount % debugOnlyWriteEvery == 0) {
          writeBuffers->addRow(
            board,
            hist,
            nextPlayer,
            turnIdx,
            (float)data.trainingWeight,
            unreducedNumVisits,
            policyTarget0,
            policyTarget1,
            data.policySurpriseByTurn[turnAfterStart],
            data.policyEntropyByTurn[turnAfterStart],
            data.searchEntropyByTurn[turnAfterStart],
            data.whiteValueTargetsByTurn,
            turnAfterStart,
            data.nnRawStatsByTurn[turnAfterStart],
            &(data.endHist.getRecentBoard(0)),
            data.finalOwnership,
            data.finalMaxLength,
            &posHistForFutureBoards,
            isSidePosition,
            numNeuralNetsBehindLatest,
            data,
            rand);
          writeAndClearIfFull();
        }
        rowCount++;
      }
      targetWeight -= 1.0;
    }

    Move move = data.endHist.moveHistory[turnIdx];
    assert(move.pla == nextPlayer);
    assert(hist.isLegal(board, move.loc, move.pla));
    hist.makeBoardMoveAssumeLegal(board, move.loc, move.pla);
    nextPlayer = getOpp(nextPlayer);
  }

  // Write side rows
  vector<ValueTargets> whiteValueTargetsBuf(1);
  for(int i = 0; i < data.sidePositions.size(); i++) {
    SidePosition* sp = data.sidePositions[i];

    double targetWeight = sp->targetWeight;
    while(targetWeight > 0.0) {
      if(targetWeight >= 1.0 || rand.nextBool(targetWeight)) {
        if(debugOut == NULL || rowCount % debugOnlyWriteEvery == 0) {
          int turnIdx = (int)sp->hist.moveHistory.size();
          assert(turnIdx >= data.startHist.moveHistory.size());
          whiteValueTargetsBuf[0] = sp->whiteValueTargets;
          bool isSidePosition = true;
          int numNeuralNetsBehindLatest = (int)data.changedNeuralNets.size() - sp->numNeuralNetChangesSoFar;

          writeBuffers->addRow(
            sp->board,
            sp->hist,
            sp->pla,
            turnIdx,
            (float)data.trainingWeight,
            sp->unreducedNumVisits,
            &(sp->policyTarget),
            NULL,
            sp->policySurprise,
            sp->policyEntropy,
            sp->searchEntropy,
            whiteValueTargetsBuf,
            0,
            sp->nnRawStats,
            NULL,
            NULL,
            NULL,
            NULL,
            isSidePosition,
            numNeuralNetsBehindLatest,
            data,
            rand);
          writeAndClearIfFull();
        }
        rowCount++;
      }
      targetWeight -= 1.0;
    }
  }
}
