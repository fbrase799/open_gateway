#include "GatewayClient.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    const auto base = qEnvironmentVariable("OGW_API", "http://127.0.0.1:8080");
    GatewayClient client(base);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("gateway", &client);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    return app.exec();
}
