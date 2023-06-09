#include "../game/boardhistory.h"

#include <algorithm>

using namespace std;


BoardHistory::BoardHistory()
  :moveHistory(),
   initialBoard(),
   initialPla(P_BLACK),
   initialTurnNumber(0),
   recentBoards(),
   numTurns(0),
   currentRecentBoardIdx(0),
   presumedNextMovePla(P_BLACK),
   isGameFinished(false),winner(C_EMPTY),isResignation(false) {}

BoardHistory::~BoardHistory() {}

BoardHistory::BoardHistory(const Board& board, Player pla)
  :moveHistory(),
   initialBoard(),
   initialPla(),
   initialTurnNumber(0),
   recentBoards(),
   numTurns(0),
   currentRecentBoardIdx(0),
   presumedNextMovePla(pla),
   isGameFinished(false),winner(C_EMPTY),isResignation(false) {
  clear(board,pla);
}

BoardHistory::BoardHistory(const BoardHistory& other)
  :moveHistory(other.moveHistory),
   initialBoard(other.initialBoard),
   initialPla(other.initialPla),
   initialTurnNumber(other.initialTurnNumber),
   recentBoards(),
   numTurns(other.numTurns),
   currentRecentBoardIdx(other.currentRecentBoardIdx),
   presumedNextMovePla(other.presumedNextMovePla),
   isGameFinished(other.isGameFinished),winner(other.winner),isResignation(other.isResignation) {
  std::copy(other.recentBoards, other.recentBoards+NUM_RECENT_BOARDS, recentBoards);
}


BoardHistory& BoardHistory::operator=(const BoardHistory& other) {
  if(this == &other)
    return *this;
  moveHistory = other.moveHistory;
  initialBoard = other.initialBoard;
  initialPla = other.initialPla;
  initialTurnNumber = other.initialTurnNumber;
  std::copy(other.recentBoards, other.recentBoards+NUM_RECENT_BOARDS, recentBoards);
  numTurns = other.numTurns;
  currentRecentBoardIdx = other.currentRecentBoardIdx;
  presumedNextMovePla = other.presumedNextMovePla;
  isGameFinished = other.isGameFinished;
  winner = other.winner;
  isResignation = other.isResignation;

  return *this;
}

BoardHistory::BoardHistory(BoardHistory&& other) noexcept
 :moveHistory(std::move(other.moveHistory)),
  initialBoard(other.initialBoard),
  initialPla(other.initialPla),
  initialTurnNumber(other.initialTurnNumber),
  recentBoards(),
  numTurns(other.numTurns),
  currentRecentBoardIdx(other.currentRecentBoardIdx),
  presumedNextMovePla(other.presumedNextMovePla),
  isGameFinished(other.isGameFinished),winner(other.winner),isResignation(other.isResignation) {
  std::copy(other.recentBoards, other.recentBoards+NUM_RECENT_BOARDS, recentBoards);
}

BoardHistory& BoardHistory::operator=(BoardHistory&& other) noexcept {
  moveHistory = std::move(other.moveHistory);
  initialBoard = other.initialBoard;
  initialPla = other.initialPla;
  initialTurnNumber = other.initialTurnNumber;
  std::copy(other.recentBoards, other.recentBoards+NUM_RECENT_BOARDS, recentBoards);
  numTurns = other.numTurns;
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

  //This makes it so that if we ask for recent boards with a lookback beyond what we have a history for,
  //we simply return copies of the starting board.
  for(int i = 0; i<NUM_RECENT_BOARDS; i++)
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

bool BoardHistory::isLegal(const Board& board, Action move, Player movePla) const {
  return board.isLegal(move, movePla);
}

void BoardHistory::makeBoardMove(Board& board, Action move, Player movePla) {
  Loc moveLoc = move.loc;
  Direction moveDir = move.dir;
  if(isLegal(board, move, movePla))
    makeBoardMoveAssumeLegal(board, move, movePla);
  else
    throw StringError("BoardHistory::makeBoardMove: Illegal move!");
  board.playMoveAssumeLegal(move, movePla);

  currentRecentBoardIdx = (currentRecentBoardIdx + 1) % NUM_RECENT_BOARDS;
  recentBoards[currentRecentBoardIdx] = board;

  moveHistory.push_back(Move(moveLoc, moveDir, movePla));
  presumedNextMovePla = getOpp(movePla);
}

void BoardHistory::makeBoardMoveAssumeLegal(Board& board, Action move, Player movePla) {
  Hash128 posHashBeforeMove = board.pos_hash;

  //If somehow we're making a move after the game was ended, just clear those values and continue
  isGameFinished = false;
  winner = C_EMPTY;
  isResignation = false;

  //Update consecutiveEndingPasses and button
  bool isSpightlikeEndingPass = false;
  //Update recent boards
  currentRecentBoardIdx = (currentRecentBoardIdx + 1) % NUM_RECENT_BOARDS;
  recentBoards[currentRecentBoardIdx] = board;
  numTurns += 1;
  
  Player nextPla = getOpp(movePla);
}
//If the game ends then the last player wins.
bool BoardHistory::checkGameEnd(const Board& board) {
  return board.checkGameEnd();
}

void BoardHistory::setWinnerByResignation(Player pla) {
  isGameFinished = true;
  isResignation = true;
  winner = pla;
}

void BoardHistory::printBasicInfo(ostream& out, const Board& board) const {
  Board::printBoard(out, board, Board::NULL_LOC, C_EMPTY, D_NONE, &moveHistory);
  out << "Next player: " << PlayerIO::playerToString(presumedNextMovePla) << endl;
}

void BoardHistory::printDebugInfo(ostream& out, const Board& board) const {
  out << board << endl;
  out << "Initial pla " << PlayerIO::playerToString(initialPla) << endl;
  out << "Turns " << numTurns << endl;
  out << "Presumed next pla " << PlayerIO::playerToString(presumedNextMovePla) << endl;
  out << "Game result " << isGameFinished << " " << PlayerIO::playerToString(winner) << isResignation << endl;
  out << "Last moves ";
  for(int i = 0; i<moveHistory.size(); i++)
    out << Location::toString(moveHistory[i].loc,board) << " ";
  out << endl;
}