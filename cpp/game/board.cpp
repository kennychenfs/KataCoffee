#include "../game/board.h"

/*
 * board.cpp
 * Originally from an unreleased project back in 2010, modified since.
 * Authors: brettharrison (original), David Wu (original and later modifications).
 */

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

#include "../core/rand.h"

using namespace std;

// STATIC VARS-----------------------------------------------------------------------------
bool Board::IS_ZOBRIST_INITALIZED = false;
Hash128 Board::ZOBRIST_SIZE_X_HASH[MAX_LEN + 1];
Hash128 Board::ZOBRIST_SIZE_Y_HASH[MAX_LEN + 1];
Hash128 Board::ZOBRIST_BOARD_HASH[MAX_ARR_SIZE][4];
Hash128 Board::ZOBRIST_PLAYER_HASH[4];
Hash128 Board::ZOBRIST_BOARD_HASH2[MAX_ARR_SIZE][4];
const Hash128 Board::ZOBRIST_GAME_IS_OVER =  // Based on sha256 hash of Board::ZOBRIST_GAME_IS_OVER
  Hash128(0xb6f9e465597a77eeULL, 0xf1d583d960a4ce7fULL);

// LOCATION--------------------------------------------------------------------------------
Spot Location::getSpot(int x, int y, int x_size) {
  return (x + 1) + (y + 1) * (x_size + 1);
}
int Location::getX(Spot loc, int x_size) {
  return (loc % (x_size + 1)) - 1;
}
int Location::getY(Spot loc, int x_size) {
  return (loc / (x_size + 1)) - 1;
}
bool Location::isAdjacent(Spot loc0, Spot loc1, int x_size) {
  return loc0 == loc1 - (x_size + 1) || loc0 == loc1 - 1 || loc0 == loc1 + 1 || loc0 == loc1 + (x_size + 1);
}

Spot Location::getMirrorSpot(Spot spot, int x_size, int y_size) {
  if(spot == Board::NULL_LOC)
    return spot;
  return getSpot(x_size - 1 - getX(spot, x_size), y_size - 1 - getY(spot, x_size), x_size);
}

Spot Location::getCenterSpot(int x_size, int y_size) {
  if(x_size % 2 == 0 || y_size % 2 == 0)
    return Board::NULL_LOC;
  return getSpot(x_size / 2, y_size / 2, x_size);
}

Spot Location::getCenterSpot(const Board& b) {
  return getCenterSpot(b.x_size, b.y_size);
}

bool Location::isCentral(Spot loc, int x_size, int y_size) {
  int x = getX(loc, x_size);
  int y = getY(loc, x_size);
  return x >= (x_size - 1) / 2 && x <= x_size / 2 && y >= (y_size - 1) / 2 && y <= y_size / 2;
}

bool Location::isNearCentral(Spot loc, int x_size, int y_size) {
  int x = getX(loc, x_size);
  int y = getY(loc, x_size);
  return x >= (x_size - 1) / 2 - 1 && x <= x_size / 2 + 1 && y >= (y_size - 1) / 2 - 1 && y <= y_size / 2 + 1;
}

#define FOREACHADJ(BLOCK) \
  { \
    int ADJOFFSET = -(x_size + 1); \
    {BLOCK}; \
    ADJOFFSET = -1; \
    {BLOCK}; \
    ADJOFFSET = 1; \
    {BLOCK}; \
    ADJOFFSET = x_size + 1; \
    {BLOCK}; \
  };
#define ADJ1 (-(x_size + 1))      // N
#define ADJ2 (-1)                 // W
#define ADJ3 (-(x_size + 1) - 1)  // NW
#define ADJ4 (-(x_size + 1) + 1)  // NE

// CONSTRUCTORS AND INITIALIZATION----------------------------------------------------------

Board::Board() {
  init(DEFAULT_LEN, DEFAULT_LEN, DEFAULT_WIN_LEN);
}

Board::Board(int x, int y, int winLen) {
  init(x, y, winLen);
}

Board::Board(int size, int winLen) {
  init(size, size, winLen);
}

Board::Board(const Board& other) {
  x_size = other.x_size;
  y_size = other.y_size;

  memcpy(colors, other.colors, sizeof(Color) * MAX_ARR_SIZE);

  pos_hash = other.pos_hash;
  lastLoc = other.lastLoc;
}

void Board::init(int xS, int yS, int winLen) {
  assert(IS_ZOBRIST_INITALIZED);
  if(xS < 0 || yS < 0 || xS > MAX_LEN || yS > MAX_LEN)
    throw StringError("Board::init - invalid board size");

  x_size = xS;
  y_size = yS;
  win_len = winLen;

  for(int i = 0; i < MAX_ARR_SIZE; i++)
    colors[i] = C_WALL;

  for(int y = 0; y < y_size; y++) {
    for(int x = 0; x < x_size; x++) {
      Spot spot = (x + 1) + (y + 1) * (x_size + 1);
      colors[spot] = C_EMPTY;
    }
  }

  pos_hash = ZOBRIST_SIZE_X_HASH[x_size] ^ ZOBRIST_SIZE_Y_HASH[y_size];
  lastLoc = Loc(Board::NULL_LOC, D_NONE);
}

void Board::initHash() {
  if(IS_ZOBRIST_INITALIZED)
    return;
  Rand rand("Board::initHash()");

  auto nextHash = [&rand]() {
    uint64_t h0 = rand.nextUInt64();
    uint64_t h1 = rand.nextUInt64();
    return Hash128(h0, h1);
  };

  for(int i = 0; i < 4; i++)
    ZOBRIST_PLAYER_HASH[i] = nextHash();

  // Do this second so that the player hash is not
  // afffected by the size of the board we compile with.
  for(int i = 0; i < MAX_ARR_SIZE; i++) {
    for(Color j = 0; j < 4; j++) {
      if(j == C_EMPTY || j == C_WALL)
        ZOBRIST_BOARD_HASH[i][j] = Hash128();
      else
        ZOBRIST_BOARD_HASH[i][j] = nextHash();
    }
  }

  // Reseed the random number generator so that these size hashes are also
  // not affected by the size of the board we compile with
  rand.init("Board::initHash() for ZOBRIST_SIZE hashes");
  for(int i = 0; i < MAX_LEN + 1; i++) {
    ZOBRIST_SIZE_X_HASH[i] = nextHash();
    ZOBRIST_SIZE_Y_HASH[i] = nextHash();
  }

  // Reseed and compute one more set of zobrist hashes, mixed a bit differently
  rand.init("Board::initHash() for second set of ZOBRIST hashes");
  for(int i = 0; i < MAX_ARR_SIZE; i++) {
    for(Color j = 0; j < 4; j++) {
      ZOBRIST_BOARD_HASH2[i][j] = nextHash();
      ZOBRIST_BOARD_HASH2[i][j].hash0 = Hash::murmurMix(ZOBRIST_BOARD_HASH2[i][j].hash0);
      ZOBRIST_BOARD_HASH2[i][j].hash1 = Hash::splitMix64(ZOBRIST_BOARD_HASH2[i][j].hash1);
    }
  }

  IS_ZOBRIST_INITALIZED = true;
}

bool Board::isOnBoard(Spot spot) const {
  return spot >= 0 && spot < MAX_ARR_SIZE && colors[spot] != C_WALL;
}

// Check if moving here is illegal.
bool Board::isLegal(Loc loc, Player pla) const {
  if(pla != P_BLACK && pla != P_WHITE)
    return false;
  if(colors[loc.spot] != C_EMPTY)
    return false;
  int lastX = Location::getX(lastLoc.spot, x_size), lastY = Location::getY(lastLoc.spot, x_size);
  int x = Location::getX(loc.spot, x_size), y = Location::getY(loc.spot, x_size);
  int dx = x - lastX, dy = y - lastY;
  switch(lastLoc.dir) {
    case D_NORTH:
      if(dx != 0 || dy == 0)
        return false;
      break;
    case D_WEST:
      if(dx == 0 || dy != 0)
        return false;
      break;
    case D_NORTHWEST:
      if(dx != dy)
        return false;
      break;
    case D_NORTHEAST:
      if(dx != -dy)
        return false;
      break;
    default:
      break;
  }
  Spot ADJS[4] = {ADJ1, ADJ2, ADJ3, ADJ4};
  Spot tempSpot = loc.spot;
  while(isOnBoard(tempSpot)) {
    tempSpot += ADJS[loc.dir];
    if(colors[tempSpot] == C_EMPTY)
      return true;
  }
  Loc tempSpot = loc;
  while(isOnBoard(tempSpot)) {
    tempSpot -= ADJS[loc.dir];
    if(colors[tempSpot] == C_EMPTY)
      return true;
  }
  return false;
}

bool Board::isEmpty() const {
  for(int y = 0; y < y_size; y++) {
    for(int x = 0; x < x_size; x++) {
      Spot spot = Location::getSpot(x, y, x_size);
      if(colors[spot] != C_EMPTY)
        return false;
    }
  }
  return true;
}

int Board::numStonesOnBoard() const {
  int num = 0;
  for(int y = 0; y < y_size; y++) {
    for(int x = 0; x < x_size; x++) {
      Spot spot = Location::getSpot(x, y, x_size);
      if(colors[spot] == C_BLACK || colors[spot] == C_WHITE)
        num += 1;
    }
  }
  return num;
}

int Board::numPlaStonesOnBoard(Player pla) const {
  int num = 0;
  for(int y = 0; y < y_size; y++) {
    for(int x = 0; x < x_size; x++) {
      Spot loc = Location::getSpot(x, y, x_size);
      if(colors[loc] == pla)
        num += 1;
    }
  }
  return num;
}

bool Board::setStone(Spot spot, Color color) {
  if(!isOnBoard(spot))
    return false;
  if(color != C_BLACK && color != C_WHITE && color != C_EMPTY)
    return false;
  colors[spot] = color;
  return true;
}
bool Board::setStones(vector<Placement> placements) {
  for(Placement placement: placements) {
    if(!setStone(placement.spot, placement.pla))
      return false;
  }
  return true;
}
// Attempts to play the specified move. Returns true if successful, returns false if the move was illegal.
bool Board::playMove(Loc loc, Player pla) {
  if(isLegal(loc, pla)) {
    playMoveAssumeLegal(loc, pla);
    return true;
  }
  return false;
}

Hash128 Board::getSitHash(Player pla) const {
  Hash128 h = pos_hash;
  h ^= Board::ZOBRIST_PLAYER_HASH[pla];
  return h;
}

// Plays the specified move, assuming it is legal, and returns a MoveRecord for the move
Board::MoveRecord Board::playMoveRecorded(Loc loc, Player pla) {
  MoveRecord record;
  record.loc = loc;
  record.pla = pla;

  playMoveAssumeLegal(loc, pla);
  return record;
}

// Undo the move given by record. Moves MUST be undone in the order they were made.
// Undos will NOT typically restore the precise representation in the board to the way it was. The heads of chains
// might change, the order of the circular lists might change, etc.
void Board::undo(Board::MoveRecord record) {
  Loc loc = record.loc;
  Spot spot = loc.spot;

  // Delete the stone played here.
  pos_hash ^= ZOBRIST_BOARD_HASH[spot][colors[spot]];
  colors[spot] = C_EMPTY;
}

bool Board::checkGameEnd() const {
  // current stones include the last move
  // If the game ends, the last player wins.
  Loc loc = lastLoc;
  Color color = colors[loc.spot];
  FOREACHADJ(
    int consecutivePla = 1; Spot adj = loc.spot + ADJOFFSET; while(isOnBoard(adj) && colors[adj] == color) {
      consecutivePla++;
      adj += ADJOFFSET;
    } adj = loc.spot - ADJOFFSET;
    while(isOnBoard(adj) && colors[adj] == color) {
      consecutivePla++;
      adj -= ADJOFFSET;
    } if(consecutivePla >= win_len) return true;);
  return false;
}

int spotToPos(Spot spot, int boardXSize, int nnXLen, int nnYLen) {
  if(spot == Board::NULL_LOC)
    return nnXLen * nnYLen;
  return Location::getY(spot, boardXSize) * nnXLen + Location::getX(spot, boardXSize);
}

// Fill pos that has a line of exact length len.
void Board::fillRowWithLine(int len, float* rowBin, int nnXLen, int nnYLen, int posStride, int featureStride) const {
  bool visited[MAX_ARR_SIZE];
  fill(visited, visited + MAX_ARR_SIZE, false);
  for(Spot spot = 0; spot < MAX_ARR_SIZE; spot++) {
    if(visited[spot] || colors[spot] == C_EMPTY)
      continue;
    visited[spot] = true;
    Color color = colors[spot];
    FOREACHADJ(
      int consecutivePla = 1; Spot adj = spot + ADJOFFSET; while(isOnBoard(adj) && colors[adj] == color) {
        visited[adj] = true;
        consecutivePla++;
        adj += ADJOFFSET;
      } adj = spot - ADJOFFSET;
      while(isOnBoard(adj) && colors[adj] == color) {
        visited[adj] = true;
        consecutivePla++;
        adj -= ADJOFFSET;
      }

      if(consecutivePla == len) {
        adj += ADJOFFSET;
        while(colors[adj] == color) {
          rowBin[spotToPos(adj, x_size, nnXLen, nnYLen) * posStride] = 1.0f;
          adj += ADJOFFSET;
        }
      });
  }
}
Hash128 Board::getPosHashAfterMove(Loc loc, Player pla) const {
  assert(loc.spot != NULL_LOC && loc.dir != D_NONE);

  return pos_hash ^ ZOBRIST_BOARD_HASH[loc.spot][pla];
}
// Plays the specified move, assuming it is legal.
void Board::playMoveAssumeLegal(Loc loc, Player pla) {
  Player opp = getOpp(pla);
  Spot spot = loc.spot;
  Direction dir = loc.dir;
  // Add the new stone as an independent group
  colors[spot] = pla;
  pos_hash ^= ZOBRIST_BOARD_HASH[spot][pla];
  lastLoc = loc;
}

int Location::distance(Spot loc0, Spot loc1, int x_size) {
  int dx = getX(loc1, x_size) - getX(loc0, x_size);
  int dy = (loc1 - loc0 - dx) / (x_size + 1);
  return (dx >= 0 ? dx : -dx) + (dy >= 0 ? dy : -dy);
}

int Location::euclideanDistanceSquared(Spot loc0, Spot loc1, int x_size) {
  int dx = getX(loc1, x_size) - getX(loc0, x_size);
  int dy = (loc1 - loc0 - dx) / (x_size + 1);
  return dx * dx + dy * dy;
}

// TACTICAL STUFF--------------------------------------------------------------------

void Board::checkConsistency() const {
  const string errLabel = string("Board::checkConsistency(): ");

  Hash128 tmp_pos_hash = ZOBRIST_SIZE_X_HASH[x_size] ^ ZOBRIST_SIZE_Y_HASH[y_size];
  for(Spot spot = 0; spot < MAX_ARR_SIZE; spot++) {
    int x = Location::getX(spot, x_size);
    int y = Location::getY(spot, x_size);
    if(x < 0 || x >= x_size || y < 0 || y >= y_size) {
      if(colors[spot] != C_WALL)
        throw StringError(errLabel + "Non-WALL value outside of board legal area");
    } else {
      if(colors[spot] == C_BLACK || colors[spot] == C_WHITE) {
        tmp_pos_hash ^= ZOBRIST_BOARD_HASH[spot][colors[spot]];
        tmp_pos_hash ^= ZOBRIST_BOARD_HASH[spot][C_EMPTY];
      } else if(colors[spot] != C_EMPTY) {
        throw StringError(errLabel + "Non-(black,white,empty) value within board legal area");
      }
    }
  }

  if(pos_hash != tmp_pos_hash)
    throw StringError(errLabel + "Pos hash does not match expected");
}

bool Board::isEqualForTesting(const Board& other, bool checkNumCaptures, bool checkSimpleKo) const {
  checkConsistency();
  other.checkConsistency();
  if(x_size != other.x_size)
    return false;
  if(y_size != other.y_size)
    return false;
  if(pos_hash != other.pos_hash)
    return false;
  for(int i = 0; i < MAX_ARR_SIZE; i++) {
    if(colors[i] != other.colors[i])
      return false;
  }
  // if(lastLoc.spot != other.lastLoc.spot || lastLoc.dir != other.lastLoc.dir)
  if(lastLoc != other.lastLoc)
    return false;
  return true;
}

// IO FUNCS------------------------------------------------------------------------------------------

// GameIO--------------------------------------------------------------------------------------------
char GameIO::colorToChar(Color c) {
  switch(c) {
    case C_BLACK:
      return 'X';
    case C_WHITE:
      return 'O';
    case C_EMPTY:
      return '.';
    default:
      return '#';
  }
}
string GameIO::colorDirectionToStringFancy(Color c, Direction d) {
  int background;
  switch(c) {
    case C_BLACK:
      background = 196;
      break;
    case C_WHITE:
      background = 33;
      break;
    case C_EMPTY:
      background = -1;
      break;
    default:
      assert(false);
  }
  string ch;
  switch(d) {
    case D_NORTH:
      ch = "|";
      break;
    case D_WEST:
      ch = "-";
      break;
    case D_NORTHEAST:
      ch = "/";
      break;
    case D_NORTHWEST:
      ch = "\\";
      break;
    case D_NONE:
      ch = " ";
      break;
    default:
      assert(false);
  }
  return ColoredOutput::colorize(ch, -1, background);
}

string GameIO::directionToString(Direction d) {
  switch(d) {
    case D_NORTH:
      return "north";
    case D_WEST:
      return "west";
    case D_NORTHEAST:
      return "northeast";
    case D_NORTHWEST:
      return "northwest";
    case D_NONE:
      return "none";
    default:
      assert(false);
  }
}

string GameIO::playerToString(Player pla) {
  switch(pla) {
    case C_BLACK:
      return "Black";
    case C_WHITE:
      return "White";
    case C_EMPTY:
      return "Empty";
    default:
      return "Wall";
  }
}

string GameIO::playerToStringShort(Player pla) {
  switch(pla) {
    case C_BLACK:
      return "B";
    case C_WHITE:
      return "W";
    case C_EMPTY:
      return "E";
    default:
      return "";
  }
}

string GameIO::moveToString(Move move, const Board& board) {
  return playerToString(move.pla) + " " + locToString(move.loc, board);
}

string GameIO::locToString(Loc loc, const Board& board) {
  return Location::toString(loc.spot, board.x_size, board.y_size) + " " + directionToString(loc.dir);
}

bool GameIO::tryParsePlayer(const string& s, Player& pla) {
  string str = Global::toLower(s);
  if(str == "black" || str == "b") {
    pla = P_BLACK;
    return true;
  } else if(str == "white" || str == "w") {
    pla = P_WHITE;
    return true;
  }
  return false;
}

Player GameIO::parsePlayer(const string& s) {
  Player pla = C_EMPTY;
  bool suc = tryParsePlayer(s, pla);
  if(!suc)
    throw StringError("Could not parse player: " + s);
  return pla;
}

bool GameIO::tryParseDirection(const std::string& s, Direction& d) {
  string str = Global::toLower(s);
  if(str == "north" || str == "n") {
    d = D_NORTH;
    return true;
  } else if(str == "west" || str == "w") {
    d = D_WEST;
    return true;
  } else if(str == "northeast" || str == "ne") {
    d = D_NORTHEAST;
    return true;
  } else if(str == "northwest" || str == "nw") {
    d = D_NORTHWEST;
    return true;
  } else if(str == "none" || str == "no" || str == "null" || str == "nil" || str == "0") {
    d = D_NONE;
    return true;
  }
  return false;
}

Direction GameIO::parseDirection(const std::string& s) {
  Direction d = D_NONE;
  bool suc = tryParseDirection(s, d);
  if(!suc)
    throw StringError("Could not parse direction: " + s);
  return d;
}

bool GameIO::tryParseLoc(const std::string& s, const Board& board, Loc& loc) {
  size_t spacePos = s.find(' ');
  if(spacePos == string::npos)
    return false;
  string locStr = s.substr(0, spacePos);
  string dirStr = s.substr(spacePos + 1);
  Spot spot;
  bool suc = Location::tryOfString(locStr, board, spot);
  if(!suc)
    return false;
  Direction dir;
  bool suc = tryParseDirection(dirStr, dir);
  if(!suc)
    return false;
  loc = Loc(spot, dir);
  return true;
}

Loc GameIO::parseLoc(const std::string& s, const Board& board) {
  Loc loc;
  bool suc = GameIO::tryParseLoc(s, board, loc);
  if(!suc)
    throw StringError("Could not parse loc: " + s);
  return loc;
}
// End of GameIO---------------------------------------------------------------------

string Location::toString(Spot spot, int x_size, int y_size) {
  if(x_size > 25 * 25)
    return toStringMach(spot, x_size);
  if(spot == Board::NULL_LOC)
    return string("null");
  const char* xChar = "ABCDEFGHJKLMNOPQRSTUVWXYZ";
  int x = getX(spot, x_size);
  int y = getY(spot, x_size);
  if(x >= x_size || x < 0 || y < 0 || y >= y_size)
    return toStringMach(spot, x_size);

  char buf[128];
  if(x <= 24)
    sprintf(buf, "%c%d", xChar[x], y_size - y);
  else
    sprintf(buf, "%c%c%d", xChar[x / 25 - 1], xChar[x % 25], y_size - y);
  return string(buf);
}
string Location::toString(Spot spot, const Board& b) {
  return toString(spot, b.x_size, b.y_size);
}

string Location::toStringMach(Spot spot, int x_size) {
  if(spot == Board::NULL_LOC)
    return string("null");
  char buf[128];
  sprintf(buf, "(%d,%d)", getX(spot, x_size), getY(spot, x_size));
  return string(buf);
}
string Location::toStringMach(Spot spot, const Board& b) {
  return toStringMach(spot, b.x_size);
}

static bool tryParseLetterCoordinate(char c, int& x) {
  if(c >= 'A' && c <= 'H')
    x = c - 'A';
  else if(c >= 'a' && c <= 'h')
    x = c - 'a';
  else if(c >= 'J' && c <= 'Z')
    x = c - 'A' - 1;
  else if(c >= 'j' && c <= 'z')
    x = c - 'a' - 1;
  else
    return false;
  return true;
}

bool Location::tryOfString(const string& str, int x_size, int y_size, Spot& result) {
  string s = Global::trim(str);
  if(s.length() < 2)
    return false;

  if(s[0] == '(') {
    if(s[s.length() - 1] != ')')
      return false;
    s = s.substr(1, s.length() - 2);
    vector<string> pieces = Global::split(s, ',');
    if(pieces.size() != 2)
      return false;
    int x;
    int y;
    bool sucX = Global::tryStringToInt(pieces[0], x);
    bool sucY = Global::tryStringToInt(pieces[1], y);
    if(!sucX || !sucY)
      return false;
    result = Location::getSpot(x, y, x_size);
    return true;
  } else {
    int x;
    if(!tryParseLetterCoordinate(s[0], x))
      return false;

    // Extended format
    if((s[1] >= 'A' && s[1] <= 'Z') || (s[1] >= 'a' && s[1] <= 'z')) {
      int x1;
      if(!tryParseLetterCoordinate(s[1], x1))
        return false;
      x = (x + 1) * 25 + x1;
      s = s.substr(2, s.length() - 2);
    } else {
      s = s.substr(1, s.length() - 1);
    }

    int y;
    bool sucY = Global::tryStringToInt(s, y);
    if(!sucY)
      return false;
    y = y_size - y;
    if(x < 0 || y < 0 || x >= x_size || y >= y_size)
      return false;
    result = Location::getSpot(x, y, x_size);
    return true;
  }
}

bool Location::tryOfStringAllowNull(const string& str, int x_size, int y_size, Spot& result) {
  if(str == "null") {
    result = Board::NULL_LOC;
    return true;
  }
  return tryOfString(str, x_size, y_size, result);
}

bool Location::tryOfString(const string& str, const Board& b, Spot& result) {
  return tryOfString(str, b.x_size, b.y_size, result);
}

bool Location::tryOfStringAllowNull(const string& str, const Board& b, Spot& result) {
  return tryOfStringAllowNull(str, b.x_size, b.y_size, result);
}

Spot Location::ofString(const string& str, int x_size, int y_size) {
  Spot result;
  if(tryOfString(str, x_size, y_size, result))
    return result;
  throw StringError("Could not parse board location: " + str);
}

Spot Location::ofStringAllowNull(const string& str, int x_size, int y_size) {
  Spot result;
  if(tryOfStringAllowNull(str, x_size, y_size, result))
    return result;
  throw StringError("Could not parse board location: " + str);
}

Spot Location::ofString(const string& str, const Board& b) {
  return ofString(str, b.x_size, b.y_size);
}

Spot Location::ofStringAllowNull(const string& str, const Board& b) {
  return ofStringAllowNull(str, b.x_size, b.y_size);
}

vector<Loc> GameIO::parseSequence(const string& str, const Board& board) {
  vector<string> pieces = Global::split(Global::trim(str), ' ');
  vector<Loc> locs;
  for(size_t i = 0; i < pieces.size(); i += 2) {
    string piece1 = Global::trim(pieces[i]);
    string piece2 = Global::trim(pieces[i + 1]);
    if(piece1.length() <= 0 || piece2.length() <= 0)
      continue;
    locs.push_back(parseLoc(piece1 + " " + piece2, board));
  }
  return locs;
}

void Board::printBoard(ostream& out, const Board& board, const vector<Move>* hist) {
  Move markMove = Move(Loc(NULL_LOC, D_NONE), C_EMPTY);

  if(hist != NULL) {
    out << "MoveNum: " << hist->size() << " ";
    markMove = (*hist)[hist->size() - 1];
  }
  out << "HASH: " << board.pos_hash << "\n";
  bool showCoords = board.x_size <= 50 && board.y_size <= 50;
  if(showCoords) {
    const char* xChar = "ABCDEFGHJKLMNOPQRSTUVWXYZ";
    out << "  ";
    for(int x = 0; x < board.x_size; x++) {
      if(x <= 24) {
        out << " ";
        out << xChar[x];
      } else {
        out << "A" << xChar[x - 25];
      }
    }
    out << "\n";
  }

  for(int y = 0; y < board.y_size; y++) {
    if(showCoords) {
      char buf[16];
      sprintf(buf, "%2d", board.y_size - y);
      out << buf << ' ';
    }
    for(int x = 0; x < board.x_size; x++) {
      Spot spot = Location::getSpot(x, y, board.x_size);
      string s = GameIO::colorDirectionToStringFancy(board.colors[spot], D_NONE);
      if(spot == markMove.loc.spot)
        out << GameIO::colorDirectionToStringFancy(markMove.pla, markMove.loc.dir);
      else
        out << s;

      bool histMarked = false;
      if(hist != NULL) {
        size_t start = hist->size() >= 3 ? hist->size() - 3 : 0;
        for(size_t i = 0; start + i < hist->size(); i++) {
          if((*hist)[start + i].loc.spot == spot) {
            out << (1 + i);
            histMarked = true;
            break;
          }
        }
      }

      if(x < board.x_size - 1 && !histMarked)
        out << ' ';
    }
    out << "\n";
  }
  out << "\n";
}
ostream& operator<<(ostream& out, const Board& board) {
  Board::printBoard(out, board, NULL);
  return out;
}

string Board::toStringSimple(const Board& board, char lineDelimiter) {
  string s;
  for(int y = 0; y < board.y_size; y++) {
    for(int x = 0; x < board.x_size; x++) {
      Spot loc = Location::getSpot(x, y, board.x_size);
      s += GameIO::colorToChar(board.colors[loc]);
    }
    s += lineDelimiter;
  }
  return s;
}

Board Board::parseBoard(int xSize, int ySize, int winLen, const string& s) {
  return parseBoard(xSize, ySize, winLen, s, '\n');
}

Board Board::parseBoard(int xSize, int ySize, int winLen, const string& s, char lineDelimiter) {
  Board board(xSize, ySize, winLen);
  vector<string> lines = Global::split(Global::trim(s), lineDelimiter);

  // Throw away coordinate labels line if it exists
  if(lines.size() == ySize + 1 && Global::isPrefix(lines[0], "A"))
    lines.erase(lines.begin());

  if(lines.size() != ySize)
    throw StringError("Board::parseBoard - string has different number of board rows than ySize");

  for(int y = 0; y < ySize; y++) {
    string line = Global::trim(lines[y]);
    // Throw away coordinates if they exist
    size_t firstNonDigitIdx = 0;
    while(firstNonDigitIdx < line.length() && Global::isDigit(line[firstNonDigitIdx]))
      firstNonDigitIdx++;
    line.erase(0, firstNonDigitIdx);
    line = Global::trim(line);

    if(line.length() != xSize && line.length() != 2 * xSize - 1)
      throw StringError("Board::parseBoard - line length not compatible with xSize");

    for(int x = 0; x < xSize; x++) {
      char c;
      if(line.length() == xSize)
        c = line[x];
      else
        c = line[x * 2];

      Spot loc = Location::getSpot(x, y, board.x_size);
      if(c == '.' || c == ' ' || c == '*' || c == ',' || c == '`')
        continue;
      else if(c == 'o' || c == 'O') {
        bool suc = board.setStone(loc, P_WHITE);
        if(!suc)
          throw StringError(string("Board::parseBoard - zero-liberty group near ") + Location::toString(loc, board));
      } else if(c == 'x' || c == 'X') {
        bool suc = board.setStone(loc, P_BLACK);
        if(!suc)
          throw StringError(string("Board::parseBoard - zero-liberty group near ") + Location::toString(loc, board));
      } else
        throw StringError(string("Board::parseBoard - could not parse board character: ") + c);
    }
  }
  return board;
}

nlohmann::json Board::toJson(const Board& board) {
  nlohmann::json data;
  data["xSize"] = board.x_size;
  data["ySize"] = board.y_size;
  data["winLen"] = board.win_len;

  data["stones"] = Board::toStringSimple(board, '|');
  data['lastLoc'] = GameIO::locToString(board.lastLoc, board);
  return data;
}

Board Board::ofJson(const nlohmann::json& data) {
  int xSize = data["xSize"].get<int>();
  int ySize = data["ySize"].get<int>();
  int winLen = data["winLen"].get<int>();
  Board board = Board::parseBoard(xSize, ySize, winLen, data["stones"].get<string>(), '|');
  board.lastLoc = GameIO::parseLoc(data["lastLoc"].get<string>(), board);
  return board;
}