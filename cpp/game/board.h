/*
 * board.h
 * Originally from an unreleased project back in 2010, modified since.
 * Authors: brettharrison (original), David Wu (original and later modifications).
 */

#ifndef GAME_BOARD_H_
#define GAME_BOARD_H_

#include "../core/global.h"
#include "../core/hash.h"
#include "../external/nlohmann_json/json.hpp"

#ifndef COMPILE_MAX_BOARD_LEN
#define COMPILE_MAX_BOARD_LEN 19
#endif

// TYPES AND CONSTANTS-----------------------------------------------------------------

struct Board;

// Simple structure for storing moves. Not used below, but this is a convenient place to define it.

STRUCT_NAMED_PAIR(Spot, spot, Direction, dir, Loc);
STRUCT_NAMED_PAIR(Loc, loc, Player, pla, Move);
STRUCT_NAMED_PAIR(Spot, spot, Player, pla, Placement);  // for sgf.cpp
// Player
typedef int8_t Player;
static constexpr Player P_BLACK = 1;
static constexpr Player P_WHITE = 2;

// Color of a point on the board
typedef int8_t Color;
static constexpr Color C_EMPTY = 0;
static constexpr Color C_BLACK = 1;
static constexpr Color C_WHITE = 2;
static constexpr Color C_WALL = 3;
static constexpr int NUM_BOARD_COLORS = 4;

// Direction(for last move)
typedef int8_t Direction;
static constexpr Direction D_NORTH = 0;
static constexpr Direction D_WEST = 1;
static constexpr Direction D_NORTHWEST = 2;
static constexpr Direction D_NORTHEAST = 3;
static constexpr Direction D_NONE = 4;
static constexpr int NUM_DIRECTIONS = 5;

static inline Color getOpp(Color c) {
  return c ^ 3;
}

// IO for players, colors, directions, and spots
namespace GameIO {
  char colorToChar(Color c);
  std::string colorDirectionToStringFancy(Color c, Direction d);
  std::string directionToString(Direction d);
  std::string playerToString(Player pla);
  std::string playerToStringShort(Player pla);
  std::string moveToString(Move move, const Board& board);
  std::string locToString(Loc loc, const Board& board);
  bool tryParsePlayer(const std::string& s, Player& pla);
  Player parsePlayer(const std::string& s);
  bool tryParseDirection(const std::string& s, Direction& d);
  Direction parseDirection(const std::string& s);
  // Loc in string should be "Spot Direction", like "A3 N" or "D1 NE"
  bool tryParseLoc(const std::string& s, const Board& board, Loc& loc);
  Loc parseLoc(const std::string& s, const Board& board);
  std::vector<Loc> parseSequence(const std::string& str, const Board& board);
}  // namespace GameIO

// Location of a point on the board
//(x,y) is represented as (x+1) + (y+1)*(x_size+1)
typedef short Spot;
namespace Location {
  Spot getSpot(int x, int y, int x_size);
  int getX(Spot spot, int x_size);
  int getY(Spot spot, int x_size);

  bool isAdjacent(Spot spot0, Spot spot1, int x_size);
  Spot getMirrorSpot(Spot spot, int x_size, int y_size);
  Spot getCenterSpot(int x_size, int y_size);
  Spot getCenterSpot(const Board& b);
  bool isCentral(Spot spot, int x_size, int y_size);
  bool isNearCentral(Spot spot, int x_size, int y_size);
  int distance(Spot spot0, Spot spot1, int x_size);
  int euclideanDistanceSquared(Spot spot0, Spot spot1, int x_size);

  std::string toString(Spot spot, int x_size, int y_size);
  std::string toString(Spot spot, const Board& b);
  std::string toStringMach(Spot spot, int x_size);
  std::string toStringMach(Spot spot, const Board& b);

  bool tryOfString(const std::string& str, int x_size, int y_size, Spot& result);
  bool tryOfString(const std::string& str, const Board& b, Spot& result);
  Spot ofString(const std::string& str, int x_size, int y_size);
  Spot ofString(const std::string& str, const Board& b);

  // Same, but will parse "null" as Board::NULL_LOC
  bool tryOfStringAllowNull(const std::string& str, int x_size, int y_size, Spot& result);
  bool tryOfStringAllowNull(const std::string& str, const Board& b, Spot& result);
  Spot ofStringAllowNull(const std::string& str, int x_size, int y_size);
  Spot ofStringAllowNull(const std::string& str, const Board& b);

}  // namespace Location

// Fast lightweight board designed for playouts and simulations, where speed is essential.
// Simple ko rule only.
// Does not enforce player turn order.

struct Board {
  // Initialization------------------------------
  // Initialize the zobrist hash.
  // MUST BE CALLED AT PROGRAM START!
  static void initHash();

  // Board parameters and Constants----------------------------------------

  static constexpr int MAX_LEN = COMPILE_MAX_BOARD_LEN;         // Maximum edge length allowed for the board
  static constexpr int DEFAULT_LEN = std::min(MAX_LEN, 5);      // Default edge length for board if unspecified
  static constexpr int DEFAULT_WIN_LEN = std::min(MAX_LEN, 4);  // Default length needed to win if unspecified
  static constexpr int MAX_PLAY_SIZE = MAX_LEN * MAX_LEN;       // Maximum number of playable spaces
  static constexpr int MAX_ARR_SIZE = (MAX_LEN + 1) * (MAX_LEN + 2) + 1;  // Maximum size of arrays needed

  // Location used to indicate an invalid spot on the board.
  static constexpr Spot NULL_LOC = 0;

  // Zobrist Hashing------------------------------
  static bool IS_ZOBRIST_INITALIZED;
  static Hash128 ZOBRIST_SIZE_X_HASH[MAX_LEN + 1];
  static Hash128 ZOBRIST_SIZE_Y_HASH[MAX_LEN + 1];
  static Hash128 ZOBRIST_BOARD_HASH[MAX_ARR_SIZE][4];
  static Hash128 ZOBRIST_BOARD_HASH2[MAX_ARR_SIZE][4];
  static Hash128 ZOBRIST_PLAYER_HASH[4];
  static const Hash128 ZOBRIST_GAME_IS_OVER;

  // Structs---------------------------------------

  // Move data passed back when moves are made to allow for undos
  struct MoveRecord {
    Player pla;
    Loc loc;
  };

  // Constructors---------------------------------
  Board();                          // Create Board of size (DEFAULT_LEN,DEFAULT_LEN)
  Board(int x, int y, int winLen);  // Create Board of size (x,y) and with win length winLen
  Board(int size, int winLen);      // Create Board of size (size, size) and with win length winLen
  Board(const Board& other);

  Board& operator=(const Board&) = default;

  // Functions------------------------------------

  // Check if moving here is legal.
  bool isLegal(Loc loc, Player pla) const;
  // Check if this location is on the board
  bool isOnBoard(Spot spot) const;
  // Is this board empty?
  bool isEmpty() const;
  // Count the number of stones on the board
  int numStonesOnBoard() const;
  int numPlaStonesOnBoard(Player pla) const;

  // Sets the specified stone if possible, including overwriting existing stones.
  // Resolves any captures and/or suicides that result from setting that stone, including deletions of the stone itself.
  // Returns false if location or color were out of range.
  bool setStone(Spot spot, Color color);
  bool setStones(vector<Placement> placements);

  // Attempts to play the specified move. Returns true if successful, returns false if the move was illegal.
  bool playMove(Loc loc, Player pla);
  // There's no handicap rule in Coffee, so use this as setStonesFailIfNoLibs in KataGo
  bool doPlacements(std::vector<Placement> placements);

  // Get a hash that combines the position of the board with a player to move.(getSitHashWithSimpleKo in KataGo)
  Hash128 Board::getSitHash(Player pla) const;
  // Plays the specified move, assuming it is legal.
  void playMoveAssumeLegal(Loc loc, Player pla);

  // Plays the specified move, assuming it is legal, and returns a MoveRecord for the move
  MoveRecord playMoveRecorded(Loc loc, Player pla);

  // Undo the move given by record. Moves MUST be undone in the order they were made.
  // Undos will NOT typically restore the precise representation in the board to the way it was. The heads of chains
  // might change, the order of the circular lists might change, etc.
  void undo(MoveRecord record);

  // Get what the position hash would be if we were to play this move and resolve captures and suicides.
  // Assumes the move is on an empty location.
  Hash128 getPosHashAfterMove(Loc loc, Player pla) const;

  bool checkGameEnd() const;
  // used in nninputs.cpp. Fill pos that has a line of exact length len.
  void fillRowWithLine(int len, float* rowBin, int nnXLen, int nnYLen, int posStride, int featureStride) const;

  // Run some basic sanity checks on the board state, throws an exception if not consistent, for testing/debugging
  void checkConsistency() const;
  // For the moment, only used in testing since it does extra consistency checks.
  // If we need a version to be used in "prod", we could make an efficient version maybe as operator==.
  bool isEqualForTesting(const Board& other, bool checkNumCaptures, bool checkSimpleKo) const;

  static Board parseBoard(int xSize, int ySize, int winLen, const std::string& s);
  static Board parseBoard(int xSize, int ySize, int winLen, const std::string& s, char lineDelimiter);
  static void printBoard(std::ostream& out, const Board& board, const std::vector<Move>* hist);

  static std::string toStringSimple(const Board& board, char lineDelimiter);
  static nlohmann::json toJson(const Board& board);
  static Board ofJson(const nlohmann::json& data);

  // Data--------------------------------------------

  int x_size;                  // Horizontal size of board
  int y_size;                  // Vertical size of board
  int win_len;                 // Number of stones in a row needed to win
  Color colors[MAX_ARR_SIZE];  // Color of each location on the board.
  Loc lastLoc;

  Hash128 pos_hash;  // A zobrist hash of the current board position (does not include ko point or player to move)

 private:
  void init(int xS, int yS, int winLen);

  friend std::ostream& operator<<(std::ostream& out, const Board& board);
};

#endif  // GAME_BOARD_H_
