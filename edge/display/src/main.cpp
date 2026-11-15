#include "GatewayClient.hpp"
#include "display_config.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    const auto base = qEnvironmentVariable("OGW_API", "http://127.0.0.1:8080");
    const auto configPath = qEnvironmentVariable("OGW_DISPLAY_CONFIG", "/etc/ogw/display.toml");
    const auto settings = load_display_settings(configPath);
    GatewayClient client(base, settings);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("gateway", &client);
    engine.rootContext()->setContextProperty("appTitle", settings.title);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    return app.exec();
}
