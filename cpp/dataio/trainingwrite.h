#ifndef DATAIO_TRAINING_WRITE_H_
#define DATAIO_TRAINING_WRITE_H_

#include "../dataio/numpywrite.h"
#include "../neuralnet/nninputs.h"
#include "../neuralnet/nninterface.h"

STRUCT_NAMED_PAIR(Loc, loc, int16_t, policyTarget, PolicyTargetMove);
STRUCT_NAMED_PAIR(std::vector<PolicyTargetMove>*, policyTargets, int64_t, unreducedNumVisits, PolicyTarget);

// Summary of value-head-related training targets for outputted data.
struct ValueTargets {
  // As usual, these are from the perspective of white.
  float win;
  float loss;

  ValueTargets();
  ~ValueTargets();
};

// Some basic extra stats to record outputted data about the neural net's raw evaluation on the position.
struct NNRawStats {
  double whiteWinLoss;
  double policyEntropy;
};

// A side position that was searched off the main line of the game, to give some data about an alternative move.
struct SidePosition {
  Board board;
  BoardHistory hist;
  Player pla;
  int64_t unreducedNumVisits;
  std::vector<PolicyTargetMove> policyTarget;
  double policySurprise;
  double policyEntropy;
  double searchEntropy;
  ValueTargets whiteValueTargets;
  NNRawStats nnRawStats;
  float targetWeight;
  float targetWeightUnrounded;
  int numNeuralNetChangesSoFar;  // Number of neural net changes this game before the creation of this side position

  SidePosition();
  SidePosition(const Board& board, const BoardHistory& hist, Player pla, int numNeuralNetChangesSoFar);
  ~SidePosition();
};

STRUCT_NAMED_PAIR(std::string, name, int, turnIdx, ChangedNeuralNet);

struct FinishedGameData {
  std::string bName;
  std::string wName;
  int bIdx;
  int wIdx;

  Board startBoard;        // Board as of the end of startHist, beginning of training period
  BoardHistory startHist;  // Board history as of start of training period
  BoardHistory endHist;    // Board history as of end of training period
  Player startPla;         // Player to move as of end of startHist.
  Hash128 gameHash;

  Player playoutDoublingAdvantagePla;
  double playoutDoublingAdvantage;
  bool hitTurnLimit;

  // Metadata about how the game was initialized
  int numExtraBlack;
  int mode;
  int beganInEncorePhase;
  int usedInitialPosition;

  // If false, then we don't have these below vectors and ownership information
  bool hasFullData;
  std::vector<float> targetWeightByTurn;
  std::vector<float> targetWeightByTurnUnrounded;
  std::vector<PolicyTarget> policyTargetsByTurn;
  std::vector<double> policySurpriseByTurn;
  std::vector<double> policyEntropyByTurn;
  std::vector<double> searchEntropyByTurn;
  std::vector<ValueTargets> whiteValueTargetsByTurn;  // Except this one, we may have some of
  std::vector<NNRawStats> nnRawStatsByTurn;
  Color* finalFullArea;
  Color* finalOwnership;
  int* finalMaxLength;

  double trainingWeight;

  std::vector<SidePosition*> sidePositions;
  std::vector<ChangedNeuralNet*> changedNeuralNets;

  double bTimeUsed;
  double wTimeUsed;
  int bMoveCount;
  int wMoveCount;

  static constexpr int NUM_MODES = 8;
  static constexpr int MODE_NORMAL = 0;
  static constexpr int MODE_CLEANUP_TRAINING = 1;
  static constexpr int MODE_FORK = 2;
  // static constexpr int MODE_HANDICAP = 3;
  static constexpr int MODE_SGFPOS = 4;
  static constexpr int MODE_HINTPOS = 5;
  static constexpr int MODE_HINTFORK = 6;
  static constexpr int MODE_ASYM = 7;

  FinishedGameData();
  ~FinishedGameData();

  void printDebug(std::ostream& out) const;
};

struct TrainingWriteBuffers {
  int inputsVersion;
  int maxRows;
  int numBinaryChannels;
  int numGlobalChannels;
  int dataXLen;
  int dataYLen;
  int packedBoardArea;

  int curRows;
  float* binaryInputNCHWUnpacked;

  // Input feature planes that have spatial extent, all of which happen to be binary.
  // Packed bitwise, with each (HW) zero-padded to a round byte.
  // Within each byte, bits are packed bigendianwise, since that's what numpy's unpackbits will expect.
  NumpyBuffer<uint8_t> binaryInputNCHWPacked;
  // Input features that are global.
  NumpyBuffer<float> globalInputNC;

  // Policy targets
  // Shape is [N,C,Pos]. Almost NCHW, except we have a Pos of length, e.g. 5x5x4=100 because Coffee's move contains spot
  // and dir, it would be more clear to put them together rather than use [N,C*4,H,W]. Contains number of visits,
  // possibly with a subtraction. Channel i will still be a dummy probability distribution (not all zero) if weight 0
  // C0: Policy target this turn.
  // C1: Policy target next turn.
  NumpyBuffer<int16_t> policyTargetsNCMove;

  // Value targets and other metadata, from the perspective of the player to move
  // C0-3->C0-1: Categorial game result, win,loss.
  // C4-7->C2-3: MCTS win-loss estimate td-like target, lambda = 1 - 1/(1 +
  // boardArea * 0.176)
  // C8-11->C4-5: MCTS win-loss estimate td-like target, lambda = 1 - 1/(1 + boardArea * 0.056)
  // C12-15->C6-7: MCTS win-loss estimate td-like target, lambda = 1 - 1/(1 + boardArea * 0.016)
  // C16-19->C8-9: MCTS win-loss estimate td-like target, lambda = 0 (so, actually just the immediate MCTS result).

  // C22->C18: Expected arrival time of WL variance.
  // C23-24: reserved for future use

  // C25->C19 Weight multiplier for row as a whole

  // C26->C20: Weight assigned to the policy target
  // C28->C21: Weight assigned to the next move policy target
  // C27->C22: Weight assigned to the final board ownership target. Most training rows will have this
  // be 1, some will be 0.
  // C30->C23: Policy Surprise (for statistical purposes)
  // C31->C24: Policy Entropy (for statistical purposes)
  // C32->C25: Search Entropy (for statistical purposes)
  // C33->C26: Weight assigned to the future position targets valueTargetsNCHW C1-C2
  // C34-35: reserved for future use

  // C36-40->C27-31: Precomputed mask values indicating if we should use historical moves 1-5, if we desire random
  // history masking. 1 means use, 0 means don't use.

  // C41-46->C32-37: 128-bit hash identifying the game, different rows from the same game share the same value.
  // Split into chunks of 22, 22, 20, 22, 22, 20 bits, little-endian style (since floats have > 22 bits of precision).

  // C49->C38: 1 if an earlier neural net started this game, compared to the latest in this data file.
  // C50->C39: If positive, an earlier neural net was playing this specific move, compared to the latest in this data
  // file.

  // C51->C40: Turn idx of the game right now, zero-indexed. Starts at 0 even for sgfposes.
  // C53->C41: First turn of this game that was selfplay for training rather than initialization (e.g. sgfposes, random
  // init of the starting board pos)

  // C55->C42: Game type, how the game was initialized
  // 0 = normal self-play game.
  // 1 = cleanup-phase-training game.
  // 2 = fork from another self-play game.
  // 4 = sampled from an external SGF position (e.g. human data or other bots).
  // 5 = sampled from a hint position (e.g. blindspot training).
  // 6 = forked from a hint position (e.g. blindspot training).
  // 7 = asymmetric playouts game (nonzero "PDA"). Note that this might actually get overwritten by modes 2,4,5,6.

  // C56->C43: Initial turn number - the turn number that corresponds to turn idx 0, such as for sgfposes.
  // C57->C44: Raw winloss from neural net
  // C59->C45: Policy prior entropy
  // C60->C46: Number of visits in the search generating this row, prior to any reduction.
  // C63->C47: Data format version, currently always equals 1.

  NumpyBuffer<float> globalTargetsNC;

  // Spatial value-related targets
  // C0: Final board ownership [-1,1], from the perspective of the player to move. All 0 if C22 has weight 0.
  // C2-3->C1-2: Future board position a certain number of turns in the future. All 0 if C33 has weight 0.
  // C3: The longest line formed at the end.
  NumpyBuffer<int8_t> valueTargetsNCHW;

  TrainingWriteBuffers(
    int inputsVersion,
    int maxRows,
    int numBinaryChannels,
    int numGlobalChannels,
    int dataXLen,
    int dataYLen);
  ~TrainingWriteBuffers();

  TrainingWriteBuffers(const TrainingWriteBuffers&) = delete;
  TrainingWriteBuffers& operator=(const TrainingWriteBuffers&) = delete;

  void clear();

  void addRow(
    const Board& board,
    const BoardHistory& hist,
    Player nextPlayer,
    int turnAfterStart,
    float targetWeight,
    int64_t unreducedNumVisits,
    const std::vector<PolicyTargetMove>* policyTarget0,  // can be null
    const std::vector<PolicyTargetMove>* policyTarget1,  // can be null
    double policySurprise,
    double policyEntropy,
    double searchEntropy,
    const std::vector<ValueTargets>& whiteValueTargets,
    int whiteValueTargetsIdx,  // index in whiteValueTargets corresponding to this turn.
    const NNRawStats& nnRawStats,
    const Board* finalBoard,
    Color* finalOwnership,
    int* finalMaxLength,
    const std::vector<Board>* posHistForFutureBoards,  // can be null
    bool isSidePosition,
    int numNeuralNetsBehindLatest,
    const FinishedGameData& data,
    Rand& rand);

  void writeToZipFile(const std::string& fileName);
  void writeToTextOstream(std::ostream& out);
};

class TrainingDataWriter {
 public:
  TrainingDataWriter(
    const std::string& outputDir,
    int inputsVersion,
    int maxRowsPerFile,
    double firstFileMinRandProp,
    int dataXLen,
    int dataYLen,
    const std::string& randSeed);
  TrainingDataWriter(
    std::ostream* debugOut,
    int inputsVersion,
    int maxRowsPerFile,
    double firstFileMinRandProp,
    int dataXLen,
    int dataYLen,
    int onlyWriteEvery,
    const std::string& randSeed);
  TrainingDataWriter(
    const std::string& outputDir,
    std::ostream* debugOut,
    int inputsVersion,
    int maxRowsPerFile,
    double firstFileMinRandProp,
    int dataXLen,
    int dataYLen,
    int onlyWriteEvery,
    const std::string& randSeed);
  ~TrainingDataWriter();

  void writeGame(const FinishedGameData& data);
  void flushIfNonempty();
  bool flushIfNonempty(std::string& resultingFilename);

  bool isEmpty() const;
  int64_t numRowsInBuffer() const;

 private:
  std::string outputDir;
  int inputsVersion;
  Rand rand;
  TrainingWriteBuffers* writeBuffers;

  std::ostream* debugOut;
  int debugOnlyWriteEvery;
  int64_t rowCount;

  bool isFirstFile;
  int firstFileMaxRows;

  void writeAndClearIfFull();
};

#endif  // DATAIO_TRAININGWRITE_H_
