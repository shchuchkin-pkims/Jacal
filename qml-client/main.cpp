#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include "gamecontroller.h"
#include "boardmodel.h"
#include "networkclient.h"
#include "updatechecker.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");
    app.setApplicationName("Jacal");
    app.setOrganizationName("Jacal");
    app.setApplicationVersion(Protocol::APP_VERSION);
    app.setWindowIcon(QIcon(":/logo.png"));

    GameController controller;
    BoardModel boardModel;
    NetworkClient networkClient;
    UpdateChecker updateChecker;

    controller.setBoardModel(&boardModel);
    controller.setNetworkClient(&networkClient);

    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty("gameController", &controller);
    engine.rootContext()->setContextProperty("boardModel", &boardModel);
    engine.rootContext()->setContextProperty("networkClient", &networkClient);
    engine.rootContext()->setContextProperty("updateChecker", &updateChecker);

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
