#pragma once
#include <QAbstractListModel>
#include "game_state.h"

class BoardModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int boardSize READ boardSize CONSTANT)

public:
    enum Roles {
        RowRole = Qt::UserRole + 1,
        ColRole,
        TileTypeNameRole,
        TileSymbolRole,
        RevealedRole,
        IsLandRole,
        IsWaterRole,
        IsCornerRole,
        CellColorRole,
        BorderColorRole,
        CoinsRole,
        HasGalleonRole,
        IsValidMoveRole,
        DirectionBitsRole,
        RotationRole,
        TreasureValueRole,
        ImageSourceRole,
        SpinnerStepsRole,
        SpinnerProgressRole,
        ImageRotationRole,
    };

    QString assetsPath() const { return m_assetsPath; }

    explicit BoardModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int boardSize() const { return BOARD_SIZE; }

    void update(const GameState& state, const PirateId& selected,
                const std::vector<Coord>& validTargets);

private:
    QString cellColor(int row, int col) const;
    QString tileColorRevealed(TileType type) const;
    QString tileImagePath(const Tile& tile) const;
    QString m_assetsPath;

    GameState m_state;
    PirateId m_selected;
    std::vector<Coord> m_validTargets;
    bool m_hasState = false;
};
