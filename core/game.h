#pragma once
#include "types.h"
#include "game_state.h"
#include "rules.h"

class Game {
public:
    Game() = default;

    void newGame(const GameConfig& config) {
        m_state = GameState{};
        initializeBoard(m_state, config);
    }

    GameState& state() { return m_state; }
    const GameState& state() const { return m_state; }

    MoveList getLegalMoves() const { return Rules::getLegalMoves(m_state); }
    EventList makeMove(const Move& move) { return Rules::applyMove(m_state, move); }

    bool isGameOver() const { return Rules::isGameOver(m_state); }
    Team getWinner() const { return Rules::getWinner(m_state); }
    Team currentTeam() const { return m_state.currentTeam(); }
    TurnPhase currentPhase() const { return m_state.phase; }
    int turnNumber() const { return m_state.turnNumber; }

private:
    GameState m_state;
};
