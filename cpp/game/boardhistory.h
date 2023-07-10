#ifndef GAME_BOARDHISTORY_H_
#define GAME_BOARDHISTORY_H_

#include "../core/global.h"
#include "../core/hash.h"
#include "../game/board.h"

// A data structure enabling checking of move legality, including optionally superko,
// and implements scoring and support for various rulesets (see rules.h)
struct BoardHistory {
  // Chronological history of moves
  std::vector<Move> moveHistory;

  // The board and player to move as of the very start, before moveHistory.
  Board initialBoard;
  Player initialPla;
  // The "turn number" as of the initial board. Does not affect any rules, but possibly uses may
  // care about this number, for cases where we set up a position from midgame.
  int64_t initialTurnNumber;

  static const int NUM_RECENT_BOARDS = 6;
  Board recentBoards[NUM_RECENT_BOARDS];
  int currentRecentBoardIdx;
  Player presumedNextMovePla;
  int numTurns;
  // Is the game supposed to be ended now?
  bool isGameFinished;
  // Winner of the game if the game is supposed to have ended now, C_EMPTY if it is a draw or isNoResult.
  Player winner;
  // True if this game is supposed to be ended but it was by resignation rather than an actual end position
  bool isResignation;

  BoardHistory();
  ~BoardHistory();

  BoardHistory(const Board& board, Player pla);

  BoardHistory(const BoardHistory& other);
  BoardHistory& operator=(const BoardHistory& other);

  BoardHistory(BoardHistory&& other) noexcept;
  BoardHistory& operator=(BoardHistory&& other) noexcept;

  // Clears all history and status and bonus points
  void clear(const Board& board, Player pla);
  // Set win_len. Affects nothing else.
  void setWinLen(float winLen);
  // Set the initial turn number. Affects nothing else.
  void setInitialTurnNumber(int64_t n);

  // Returns a copy of this board history rewound to the initial board, pla, etc, with other fields
  //(such as setInitialTurnNumber) set identically.
  BoardHistory copyToInitial() const;

  // Returns a reference a recent board state, where 0 is the current board, 1 is 1 move ago, etc.
  // Requires that numMovesAgo < NUM_RECENT_BOARDS
  const Board& getRecentBoard(int numMovesAgo) const;

  // Check if a move on the board is legal, taking into account the full game state and superko
  bool isLegal(const Board& board, Loc moveLoc, Player movePla) const;
  // raise error if not legal
  void makeBoardMove(Board& board, Loc moveLoc, Player movePla);
  void makeBoardMoveAssumeLegal(Board& board, Loc moveLoc, Player movePla);

  int64_t getCurrentTurnNumber() const;
  Hash128 getSituationHash(const Board& board, Player nextPlayer);
  bool checkGameEnd(const Board& board);
  Player getWinner(const Board& board) const;
  void setWinnerByResignation(Player pla);

  void printBasicInfo(std::ostream& out, const Board& board) const;
  void printDebugInfo(std::ostream& out, const Board& board) const;
};

#endif  // GAME_BOARDHISTORY_H_
