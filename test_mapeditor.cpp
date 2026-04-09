#include "core/map_def.h"
#include <iostream>
#include <cassert>

int main() {
    // Test 1: Create and save a map
    MapDefinition md;
    md.name = "Test Island";
    md.id = "test_island";
    md.minPlayers = 2;
    md.maxPlayers = 3;
    
    // Fill with sea
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++)
            md.terrain[r][c] = Terrain::Sea;
    
    // Add land (small island)
    for (int r = 3; r <= 9; r++)
        for (int c = 3; c <= 9; c++)
            md.terrain[r][c] = Terrain::Land;
    
    // Add some rocks
    md.terrain[5][5] = Terrain::Rock;
    md.terrain[6][6] = Terrain::Rock;
    
    // Ship spawns
    md.ships.push_back({0, 2, 6}); // White north
    md.ships.push_back({1, 10, 6}); // Yellow south
    md.ships.push_back({2, 6, 2}); // Black west
    
    std::cout << "Land cells: " << md.countLandCells() << std::endl;
    assert(md.countLandCells() == 47); // 7x7=49 - 2 rocks
    
    // Save
    std::string path = "/tmp/test_island.jmap";
    bool saved = saveMapToFile(md, path);
    std::cout << "Save: " << (saved ? "OK" : "FAIL") << std::endl;
    assert(saved);
    
    // Test 2: Load the map back
    MapDefinition loaded;
    bool ok = loadMapFromFile(path, loaded);
    std::cout << "Load: " << (ok ? "OK" : "FAIL") << std::endl;
    assert(ok);
    
    // Verify
    assert(loaded.name == "Test Island");
    assert(loaded.id == "test_island");
    assert(loaded.minPlayers == 2);
    assert(loaded.maxPlayers == 3);
    assert(loaded.ships.size() == 3);
    assert(loaded.ships[0].team == 0);
    assert(loaded.ships[0].row == 2);
    assert(loaded.ships[0].col == 6);
    assert(loaded.countLandCells() == 47);
    assert(loaded.terrain[5][5] == Terrain::Rock);
    assert(loaded.terrain[3][3] == Terrain::Land);
    assert(loaded.terrain[0][0] == Terrain::Sea);
    
    std::cout << "All terrain matches: OK" << std::endl;
    
    // Test 3: Print saved file
    std::cout << "\n=== Saved file content ===" << std::endl;
    FILE* f = fopen(path.c_str(), "r");
    char buf[256];
    while (fgets(buf, sizeof(buf), f)) std::cout << buf;
    fclose(f);
    
    // Test 4: loadCustomMaps from directory
    // Save another map
    md.name = "Tiny"; md.id = "tiny";
    saveMapToFile(md, "/tmp/tiny.jmap");
    auto customs = loadCustomMaps("/tmp");
    std::cout << "\nCustom maps found: " << customs.size() << std::endl;
    bool foundTest = false, foundTiny = false;
    for (auto& m : customs) {
        std::cout << "  - " << m.id << " (" << m.name << "), land=" << m.countLandCells() 
                  << ", ships=" << m.ships.size() << std::endl;
        if (m.id == "test_island") foundTest = true;
        if (m.id == "tiny") foundTiny = true;
    }
    assert(foundTest && foundTiny);
    
    std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
    return 0;
}
