#include "types.h"

const char* tileTypeName(TileType t) {
    switch (t) {
        case TileType::Sea:           return "Sea";
        case TileType::Empty:         return "Empty";
        case TileType::Arrow:         return "Arrow";
        case TileType::Horse:         return "Horse";
        case TileType::Jungle:        return "Jungle";
        case TileType::Desert:        return "Desert";
        case TileType::Swamp:         return "Swamp";
        case TileType::Mountain:      return "Mountain";
        case TileType::Ice:           return "Ice";
        case TileType::Trap:          return "Trap";
        case TileType::Crocodile:     return "Crocodile";
        case TileType::Cannibal:      return "Cannibal";
        case TileType::Fortress:      return "Fortress";
        case TileType::ResurrectFort: return "ResurrectFort";
        case TileType::Treasure:      return "Treasure";
        case TileType::Galleon:       return "Galleon";
        case TileType::Airplane:      return "Airplane";
        case TileType::Lighthouse:    return "Lighthouse";
        case TileType::Earthquake:    return "Earthquake";
        case TileType::Balloon:       return "Balloon";
        case TileType::Cannon:        return "Cannon";
        case TileType::BenGunn:       return "BenGunn";
        case TileType::Missionary:    return "Missionary";
        case TileType::Friday:        return "Friday";
        case TileType::Rum:           return "Rum";
        case TileType::RumBarrel:     return "RumBarrel";
        case TileType::Cave:          return "Cave";
        case TileType::ThickJungle:   return "ThickJungle";
        case TileType::Grass:         return "Grass";
        case TileType::Caramba:       return "Caramba";
        default: return "?";
    }
}

const char* tileSymbol(TileType t) {
    switch (t) {
        case TileType::Sea:           return "~";
        case TileType::Empty:         return "";
        case TileType::Arrow:         return ">";
        case TileType::Horse:         return "L";
        case TileType::Jungle:        return "J2";
        case TileType::Desert:        return "D3";
        case TileType::Swamp:         return "S4";
        case TileType::Mountain:      return "M5";
        case TileType::Ice:           return "**";
        case TileType::Trap:          return "##";
        case TileType::Crocodile:     return "<>";
        case TileType::Cannibal:      return "XX";
        case TileType::Fortress:      return "[]";
        case TileType::ResurrectFort: return "[+]";
        case TileType::Treasure:      return "$";
        case TileType::Galleon:       return "$$$";
        case TileType::Airplane:      return "AP";
        case TileType::Lighthouse:    return "LH";
        case TileType::Earthquake:    return "EQ";
        case TileType::Balloon:       return "BL";
        case TileType::Cannon:        return "CN";
        case TileType::BenGunn:       return "BG";
        case TileType::Missionary:    return "MI";
        case TileType::Friday:        return "FR";
        case TileType::Rum:           return "RM";
        case TileType::RumBarrel:     return "RB";
        case TileType::Cave:          return "CV";
        case TileType::ThickJungle:   return "TJ";
        case TileType::Grass:         return "GR";
        case TileType::Caramba:       return "!!";
        default: return "?";
    }
}
