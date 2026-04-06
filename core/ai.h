#pragma once
#include "types.h"
#include "game_state.h"

namespace AI {

// Choose the best move for the current player using heuristic evaluation
Move chooseBestMove(const GameState& state, const MoveList& legalMoves);

// Evaluate game state from a team's perspective (higher = better)
int evaluateState(const GameState& state, Team team);

// Evaluate a single move's immediate value
int evaluateMove(const GameState& state, const Move& move, Team team);

} // namespace AI
