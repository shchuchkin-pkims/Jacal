#include "core/game.h"
#include <iostream>
int main() {
    GameConfig cfg; cfg.numTeams = 2; cfg.seed = 1;
    Game game; game.newGame(cfg);
    auto& s = game.state();
    Coord c = {12, 6};
    std::cout << "(12,6) isWater=" << s.mapIsWater(c) << " isLand=" << s.mapIsLand(c) << " shipAt=" << s.shipIndexAt(c) << std::endl;
    Coord c2 = {12, 5};
    std::cout << "(12,5) isWater=" << s.mapIsWater(c2) << " isLand=" << s.mapIsLand(c2) << std::endl;
    std::cout << "ship[1].pos=(" << s.ships[1].pos.row << "," << s.ships[1].pos.col << ")" << std::endl;
    return 0;
}
