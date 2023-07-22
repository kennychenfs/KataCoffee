#include "../game/boardhistory.h"

#include <algorithm>

using namespace std;

BoardHistory::BoardHistory()
  : moveHistory(),
    initialBoard(),
    initialPla(P_BLACK),
    initialTurnNumber(0),
    recentBoards(),
    numTurns(0),
    currentRecentBoardIdx(0),
    presumedNextMovePla(P_BLACK),
    isGameFinished(false),
    winner(C_EMPTY),
    isResignation(false) {}

BoardHistory::~BoardHistory() {}

BoardHistory::BoardHistory(const Board& board, Player pla)
  : moveHistory(),
    initialBoard(),
    initialPla(),
    initialTurnNumber(0),
    recentBoards(),
    currentRecentBoardIdx(0),
    presumedNextMovePla(pla),
    isGameFinished(false),
    winner(C_EMPTY),
    isResignation(false) {
  clear(board, pla);
}

BoardHistory::BoardHistory(const BoardHistory& other)
  : moveHistory(other.moveHistory),
    initialBoard(other.initialBoard),
    initialPla(other.initialPla),
    initialTurnNumber(other.initialTurnNumber),
    recentBoards(),
    numTurns(other.numTurns),
    currentRecentBoardIdx(other.currentRecentBoardIdx),
    presumedNextMovePla(other.presumedNextMovePla),
    isGameFinished(other.isGameFinished),
    winner(other.winner),
    isResignation(other.isResignation) {
  std::copy(other.recentBoards, other.recentBoards + NUM_RECENT_BOARDS, recentBoards);
}

BoardHistory& BoardHistory::operator=(const BoardHistory& other) {
  if(this == &other)
    return *this;
  moveHistory = other.moveHistory;
  initialBoard = other.initialBoard;
  initialPla = other.initialPla;
  initialTurnNumber = other.initialTurnNumber;
  std::copy(other.recentBoards, other.recentBoards + NUM_RECENT_BOARDS, recentBoards);
  numTurns = other.numTurns;
  currentRecentBoardIdx = other.currentRecentBoardIdx;
  presumedNextMovePla = other.presumedNextMovePla;
  isGameFinished = other.isGameFinished;
  winner = other.winner;
  isResignation = other.isResignation;

  return *this;
}

BoardHistory::BoardHistory(BoardHistory&& other) noexcept
  : moveHistory(std::move(other.moveHistory)),
    initialBoard(other.initialBoard),
    initialPla(other.initialPla),
    initialTurnNumber(other.initialTurnNumber),
    recentBoards(),
    numTurns(other.numTurns),
    currentRecentBoardIdx(other.currentRecentBoardIdx),
    presumedNextMovePla(other.presumedNextMovePla),
    isGameFinished(other.isGameFinished),
    winner(other.winner),
    isResignation(other.isResignation) {
  std::copy(other.recentBoards, other.recentBoards + NUM_RECENT_BOARDS, recentBoards);
}

BoardHistory& BoardHistory::operator=(BoardHistory&& other) noexcept {
  moveHistory = std::move(other.moveHistory);
  initialBoard = other.initialBoard;
  initialPla = other.initialPla;
  initialTurnNumber = other.initialTurnNumber;
  std::copy(other.recentBoards, other.recentBoards + NUM_RECENT_BOARDS, recentBoards);
  currentRecentBoardIdx = other.currentRecentBoardIdx;
  presumedNextMovePla = other.presumedNextMovePla;
  isGameFinished = other.isGameFinished;
  winner = other.winner;
  isResignation = other.isResignation;

  return *this;
}

void BoardHistory::clear(const Board& board, Player pla) {
  moveHistory.clear();

  initialBoard = board;
  initialPla = pla;
  initialTurnNumber = 0;

  // This makes it so that if we ask for recent boards with a lookback beyond what we have a history for,
  // we simply return copies of the starting board.
  for(int i = 0; i < NUM_RECENT_BOARDS; i++)
    recentBoards[i] = board;

  numTurns = 0;

  currentRecentBoardIdx = 0;

  presumedNextMovePla = pla;

  isGameFinished = false;
  winner = C_EMPTY;
  isResignation = false;
}

void BoardHistory::setInitialTurnNumber(int64_t n) {
  initialTurnNumber = n;
}

BoardHistory BoardHistory::copyToInitial() const {
  BoardHistory hist(initialBoard, initialPla);
  hist.setInitialTurnNumber(initialTurnNumber);
  return hist;
}

const Board& BoardHistory::getRecentBoard(int numMovesAgo) const {
  assert(numMovesAgo >= 0 && numMovesAgo < NUM_RECENT_BOARDS);
  int idx = (currentRecentBoardIdx - numMovesAgo + NUM_RECENT_BOARDS) % NUM_RECENT_BOARDS;
  return recentBoards[idx];
}

bool BoardHistory::isLegal(const Board& board, Loc moveLoc, Player movePla) const {
  return board.isLegal(moveLoc, movePla);
}

void BoardHistory::makeBoardMove(Board& board, Loc moveLoc, Player movePla) {
  Spot spot = moveLoc.spot;
  Direction dir = moveLoc.dir;
  if(isLegal(board, moveLoc, movePla))
    makeBoardMoveAssumeLegal(board, moveLoc, movePla);
  else
    throw StringError("BoardHistory::makeBoardMove: Illegal move!");

  currentRecentBoardIdx = (currentRecentBoardIdx + 1) % NUM_RECENT_BOARDS;
  recentBoards[currentRecentBoardIdx] = board;

  moveHistory.push_back(Move(moveLoc, movePla));
  presumedNextMovePla = getOpp(movePla);
}

void BoardHistory::makeBoardMoveAssumeLegal(Board& board, Loc moveLoc, Player movePla) {
  // If somehow we're making a move after the game was ended, just clear those values and continue
  isGameFinished = false;
  winner = C_EMPTY;
  isResignation = false;

  board.playMoveAssumeLegal(moveLoc, movePla);
  // Update consecutiveEndingPasses and button
  bool isSpightlikeEndingPass = false;
  // Update recent boards
  currentRecentBoardIdx = (currentRecentBoardIdx + 1) % NUM_RECENT_BOARDS;
  recentBoards[currentRecentBoardIdx] = board;
  numTurns += 1;

  Player nextPla = getOpp(movePla);
  if(board.checkGameEnd()) {
    isGameFinished = true;
    winner = movePla;
  }
}
int64_t BoardHistory::getCurrentTurnNumber() const {
  return std::max((int64_t)0, initialTurnNumber + (int64_t)moveHistory.size());
}
// If the game ends then the last player wins.
bool BoardHistory::checkGameEnd(const Board& board) {
  return board.checkGameEnd();
}

Hash128 BoardHistory::getSituationHash(const Board& board, Player nextPlayer) {
  // Note that board.pos_hash also incorporates the size of the board.
  Hash128 hash = board.pos_hash;
  hash ^= Board::ZOBRIST_PLAYER_HASH[nextPlayer];
  return hash;
}

bool BoardHistory::checkGameEnd(const Board& board) {
  if(board.checkGameEnd()) {
    winner = moveHistory[moveHistory.size() - 1].pla;
    return true;
  }
  return false;
}
Player BoardHistory::getWinner(const Board& board) const {
  return winner;
}
void BoardHistory::setWinnerByResignation(Player pla) {
  isGameFinished = true;
  isResignation = true;
  winner = pla;
}

void BoardHistory::printBasicInfo(ostream& out, const Board& board) const {
  Board::printBoard(out, board, &moveHistory);
  out << "Next player: " << GameIO::playerToString(presumedNextMovePla) << endl;
}

void BoardHistory::printDebugInfo(ostream& out, const Board& board) const {
  out << board << endl;
  out << "Initial pla " << GameIO::playerToString(initialPla) << endl;
  out << "Turns " << numTurns << endl;
  out << "Presumed next pla " << GameIO::playerToString(presumedNextMovePla) << endl;
  out << "Game result " << isGameFinished << " " << GameIO::playerToString(winner) << isResignation << endl;
  out << "Last moves ";
  for(int i = 0; i < moveHistory.size(); i++)
    out << GameIO::locToString(moveHistory[i].loc, board) << " ";
  out << endl;
}