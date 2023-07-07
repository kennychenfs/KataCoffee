#include "../game/graphhash.h"

Hash128 GraphHash::getStateHash(const BoardHistory& hist, Player nextPlayer) {
  const Board& board = hist.getRecentBoard(0);
  Hash128 hash = board.getSitHash(nextPlayer);

  // Fold in whether the game is over or not
  if(hist.isGameFinished)
    hash ^= Board::ZOBRIST_GAME_IS_OVER;

  return hash;
}

Hash128 GraphHash::getGraphHash(Hash128 prevGraphHash, const BoardHistory& hist, Player nextPlayer) {
  const Board& board = hist.getRecentBoard(0);
  Loc prevMoveLoc = hist.moveHistory.size() <= 0 ? Board::NULL_LOC : hist.moveHistory[hist.moveHistory.size()-1].loc;
  if(prevMoveLoc == Board::NULL_LOC) {
    return getStateHash(hist,nextPlayer);
  }
  else {
    Hash128 newHash = prevGraphHash;
    newHash.hash0 = Hash::splitMix64(newHash.hash0 ^ newHash.hash1);
    newHash.hash1 = Hash::nasam(newHash.hash1) + newHash.hash0;
    Hash128 stateHash = getStateHash(hist,nextPlayer);
    newHash.hash0 += stateHash.hash0;
    newHash.hash1 += stateHash.hash1;
    return newHash;
  }
}

Hash128 GraphHash::getGraphHashFromScratch(const BoardHistory& histOrig, Player nextPlayer, int repBound, double drawEquivalentWinsForWhite) {
  BoardHistory hist = histOrig.copyToInitial();
  Board board = hist.getRecentBoard(0);
  Hash128 graphHash = Hash128();

  for(size_t i = 0; i<histOrig.moveHistory.size(); i++) {
    graphHash = getGraphHash(graphHash, hist, histOrig.moveHistory[i].pla);
    hist.makeBoardMoveAssumeLegal(board, moveToAction(histOrig.moveHistory[i]), histOrig.moveHistory[i].pla);
  }
  assert(
    board.pos_hash ==
    histOrig.getRecentBoard(0).pos_hash
  );

  graphHash = getGraphHash(graphHash, hist, nextPlayer);
  return graphHash;
}

