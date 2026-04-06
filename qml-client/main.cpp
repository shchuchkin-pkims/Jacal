#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "gamecontroller.h"
#include "boardmodel.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("Jackal");
    app.setOrganizationName("Jackal");

    GameController controller;
    BoardModel boardModel;
    controller.setBoardModel(&boardModel);

    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty("gameController", &controller);
    engine.rootContext()->setContextProperty("boardModel", &boardModel);

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
