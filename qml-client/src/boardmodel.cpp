#include "boardmodel.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

BoardModel::BoardModel(QObject* parent) : QAbstractListModel(parent) {
    // Find assets/tiles/ relative to executable
    QString appDir = QCoreApplication::applicationDirPath();
    // Try several paths
    QStringList candidates = {
        appDir + "/../../assets/tiles",      // build/qml-client/../../assets/tiles
        appDir + "/../assets/tiles",
        appDir + "/assets/tiles",
        QDir::homePath() + "/Documents/VS_Code/Jacal/assets/tiles",
    };
    for (auto& p : candidates) {
        QDir d(p);
        if (d.exists()) {
            m_assetsPath = d.absolutePath();
            break;
        }
    }
}

int BoardModel::rowCount(const QModelIndex&) const {
    return BOARD_SIZE * BOARD_SIZE;
}

QHash<int, QByteArray> BoardModel::roleNames() const {
    return {
        {RowRole,            "tileRow"},
        {ColRole,            "tileCol"},
        {TileTypeNameRole,   "tileTypeName"},
        {TileSymbolRole,     "tileSymbol"},
        {RevealedRole,       "revealed"},
        {IsLandRole,         "isLand"},
        {IsWaterRole,        "isWater"},
        {IsCornerRole,       "isCorner"},
        {CellColorRole,      "cellColor"},
        {BorderColorRole,    "borderColor"},
        {CoinsRole,          "coins"},
        {HasGalleonRole,     "hasGalleon"},
        {IsValidMoveRole,    "isValidMove"},
        {DirectionBitsRole,  "directionBits"},
        {RotationRole,       "tileRotation"},
        {TreasureValueRole,  "treasureValue"},
        {ImageSourceRole,    "imageSource"},
        {SpinnerStepsRole,   "spinnerSteps"},
        {SpinnerProgressRole,"spinnerProgress"},
        {ImageRotationRole,  "imageRotation"},
    };
}

QVariant BoardModel::data(const QModelIndex& index, int role) const {
    int idx = index.row();
    int r = idx / BOARD_SIZE;
    int c = idx % BOARD_SIZE;
    Coord coord = {r, c};

    switch (role) {
    case RowRole: return r;
    case ColRole: return c;
    case IsCornerRole: return isCorner(coord);
    case IsWaterRole: return isWater(coord) || isCorner(coord);
    case IsLandRole: return isLand(coord);

    case RevealedRole:
        if (!m_hasState || !isLand(coord)) return true;
        return m_state.board[r][c].revealed;

    case TileTypeNameRole:
        if (!m_hasState || !isLand(coord)) return "";
        if (!m_state.board[r][c].revealed) return "closed";
        return QString::fromUtf8(tileTypeName(m_state.board[r][c].type));

    case TileSymbolRole:
        if (!m_hasState || !isLand(coord)) return "";
        if (!m_state.board[r][c].revealed) return "";
        return QString::fromUtf8(tileSymbol(m_state.board[r][c].type));

    case CellColorRole: return cellColor(r, c);

    case BorderColorRole: {
        for (auto& vt : m_validTargets) {
            if (vt.row == r && vt.col == c) return QString("#00ff00");
        }
        return QString("#40404040");
    }

    case CoinsRole:
        if (!m_hasState || !isLand(coord)) return 0;
        return m_state.board[r][c].coins;

    case HasGalleonRole:
        if (!m_hasState || !isLand(coord)) return false;
        return m_state.board[r][c].hasGalleonTreasure;

    case IsValidMoveRole: {
        for (auto& vt : m_validTargets) {
            if (vt.row == r && vt.col == c) return true;
        }
        return false;
    }

    case DirectionBitsRole:
        if (!m_hasState || !isLand(coord)) return 0;
        if (!m_state.board[r][c].revealed) return 0;
        return static_cast<int>(m_state.board[r][c].directionBits);

    case RotationRole:
        if (!m_hasState || !isLand(coord)) return 0;
        return m_state.board[r][c].rotation;

    case TreasureValueRole:
        if (!m_hasState || !isLand(coord)) return 0;
        return m_state.board[r][c].treasureValue;

    case ImageSourceRole: {
        if (!m_hasState || !isLand(coord) || m_assetsPath.isEmpty()) return QString("");
        const auto& tile = m_state.board[r][c];
        if (!tile.revealed) return QString("file://" + m_assetsPath + "/closed.png");
        return QString("file://" + tileImagePath(tile));
    }

    case SpinnerStepsRole:
        if (!m_hasState || !isLand(coord)) return 0;
        if (!m_state.board[r][c].revealed) return 0;
        return spinnerSteps(m_state.board[r][c].type);

    case SpinnerProgressRole: {
        if (!m_hasState || !isLand(coord)) return 0;
        if (!isSpinner(m_state.board[r][c].type)) return 0;
        int maxProg = 0;
        for (int t = 0; t < m_state.config.numTeams; t++)
            for (int i = 0; i < PIRATES_PER_TEAM; i++) {
                auto& p = m_state.pirates[t][i];
                if (p.pos.row == r && p.pos.col == c && p.state == PirateState::OnBoard)
                    if (p.spinnerProgress > maxProg) maxProg = p.spinnerProgress;
            }
        return maxProg;
    }

    case ImageRotationRole: {
        if (!m_hasState || !isLand(coord)) return 0.0;
        auto& tile = m_state.board[r][c];
        if (!tile.revealed) return 0.0;

        if (tile.type == TileType::Arrow) {
            int ndirs = popcount8(tile.directionBits);
            if (ndirs == 1) {
                // Single-direction: image points East (right)
                for (int d = 0; d < 8; d++) {
                    if (tile.directionBits & dirBit(d)) {
                        return static_cast<double>((d - DIR_E) * 45);
                    }
                }
            }
            if (ndirs == 2) {
                // 2-dir opposite: tile_arrow_2b.png shows E+W (horizontal)
                // Find first direction to compute rotation from horizontal
                for (int d = 0; d < 8; d++) {
                    if (tile.directionBits & dirBit(d)) {
                        // d=0(N): need 90°, d=2(E): need 0°, d=4(S): need 90°
                        return static_cast<double>((d - DIR_E) * 45);
                    }
                }
            }
            // 4-direction: use stored rotation
            return static_cast<double>(tile.rotation * 90);
        }
        if (tile.type == TileType::Cannon) {
            // Cannon image points North (up) in the PDF
            for (int d = 0; d < 8; d++) {
                if (tile.directionBits & dirBit(d)) {
                    return static_cast<double>(d * 45);
                }
            }
        }
        return 0.0;
    }

    default: return {};
    }
}

void BoardModel::update(const GameState& state, const PirateId& selected,
                        const std::vector<Coord>& validTargets) {
    m_state = state;
    m_selected = selected;
    m_validTargets = validTargets;
    m_hasState = true;
    emit dataChanged(index(0), index(BOARD_SIZE * BOARD_SIZE - 1));
}

QString BoardModel::cellColor(int r, int c) const {
    Coord coord = {r, c};
    if (isCorner(coord)) return "#0a1e3d";
    if (isWater(coord)) return "#1a6090";
    if (!m_hasState) return "#2d5a1e";
    const auto& tile = m_state.board[r][c];
    if (!tile.revealed) return "#2d5a1e";
    return tileColorRevealed(tile.type);
}

QString BoardModel::tileColorRevealed(TileType type) const {
    switch (type) {
    case TileType::Empty:         return "#4a8a3a";
    case TileType::Arrow:         return "#d4c060";
    case TileType::Horse:         return "#d4c060";
    case TileType::Jungle:        return "#1a6a20";
    case TileType::Desert:        return "#d4a040";
    case TileType::Swamp:         return "#3a6a4a";
    case TileType::Mountain:      return "#8a8a8a";
    case TileType::Ice:           return "#a0d0f0";
    case TileType::Trap:          return "#8a4a2a";
    case TileType::Crocodile:     return "#5a8a3a";
    case TileType::Cannibal:      return "#6a2020";
    case TileType::Fortress:      return "#707070";
    case TileType::ResurrectFort: return "#608060";
    case TileType::Treasure:      return "#c0a030";
    case TileType::Galleon:       return "#c09020";
    case TileType::Airplane:      return "#70a0c0";
    case TileType::Lighthouse:    return "#c0c0a0";
    case TileType::Earthquake:    return "#a07050";
    case TileType::Balloon:       return "#b0a0d0";
    case TileType::Cannon:        return "#607050";
    case TileType::BenGunn:       return "#4a9a3a";
    case TileType::Missionary:    return "#4a7aba";
    case TileType::Friday:        return "#8a6a3a";
    case TileType::Rum:           return "#c09050";
    case TileType::RumBarrel:     return "#b08040";
    case TileType::Cave:          return "#4a4a4a";
    case TileType::ThickJungle:   return "#0a4a10";
    case TileType::Grass:         return "#60aa40";
    case TileType::Caramba:       return "#c06040";
    default: return "#4a8a3a";
    }
}

QString BoardModel::tileImagePath(const Tile& tile) const {
    if (m_assetsPath.isEmpty()) return "";

    QString base = m_assetsPath + "/";
    switch (tile.type) {
    case TileType::Empty:
        return base + QString("empty_%1.png").arg(tile.visualVariant % 5);
    case TileType::Arrow: {
        int ndirs = popcount8(tile.directionBits);
        if (ndirs == 1) return base + "arrow_1dir.png";
        if (ndirs == 2) {
            bool opposite = false;
            for (int d = 0; d < 4; d++)
                if (tile.directionBits == (dirBit(d) | dirBit((d + 4) % 8)))
                    { opposite = true; break; }
            return base + (opposite ? "arrow_2dir_opposite.png" : "arrow_2dir_perp.png");
        }
        if (ndirs == 4) {
            if (tile.directionBits == DIRS_CARDINAL) return base + "arrow_4dir_cardinal.png";
            return base + "arrow_4dir_diagonal.png";
        }
        return base + "arrow_1dir.png";
    }
    case TileType::Horse:         return base + "horse.png";
    case TileType::Jungle:        return base + "jungle.png";
    case TileType::Desert:        return base + "desert.png";
    case TileType::Swamp:         return base + "swamp.png";
    case TileType::Mountain:      return base + "mountain.png";
    case TileType::Ice:           return base + "ice.png";
    case TileType::Trap:          return base + "trap.png";
    case TileType::Crocodile:     return base + "crocodile.png";
    case TileType::Cannibal:      return base + "cannibal.png";
    case TileType::Fortress:      return base + "fortress.png";
    case TileType::ResurrectFort: return base + "resurrect_fort.png";
    case TileType::Treasure: {
        const char* roman[] = {"", "I", "II", "III", "IV", "V"};
        int v = tile.treasureValue;
        if (v >= 1 && v <= 5)
            return base + QString("treasure_%1.png").arg(roman[v]);
        return base + "treasure_I.png";
    }
    case TileType::Galleon:       return base + "galleon.png";
    case TileType::Airplane:      return base + "airplane.png";
    case TileType::Lighthouse:    return base + "lighthouse.png";
    case TileType::Earthquake:    return base + "earthquake.png";
    case TileType::Balloon:       return base + "balloon.png";
    case TileType::Cannon:        return base + "cannon.png";
    case TileType::BenGunn:       return base + "bengunn.png";
    case TileType::Missionary:    return base + "missionary.png";
    case TileType::Friday:        return base + "friday.png";
    case TileType::Rum:           return base + "rum.png";
    case TileType::RumBarrel:     return base + "rum_barrel.png";
    case TileType::Cave:          return base + "cave.png";
    case TileType::ThickJungle:   return base + "thick_jungle.png";
    case TileType::Grass:         return base + "grass.png";
    case TileType::Caramba:       return base + "caramba.png";
    default:                      return base + "empty_0.png";
    }
}
