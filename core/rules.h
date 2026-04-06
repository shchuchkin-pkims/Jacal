#pragma once
#include "types.h"
#include "game_state.h"

namespace Rules {

// Get all legal moves for the current player/phase
MoveList getLegalMoves(const GameState& state);

// Apply a move, mutate state, return events for UI
EventList applyMove(GameState& state, const Move& move);

// Check game-over: all coins scored or unreachable
bool isGameOver(const GameState& state);

// Winner (team with most coins)
Team getWinner(const GameState& state);

// Coins still on the board (on tiles, carried by pirates)
int coinsRemaining(const GameState& state);

} // namespace Rules
