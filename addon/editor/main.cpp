#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "MappingListModel.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("schnelle-umlaute-editor"));
    QGuiApplication::setOrganizationName(QStringLiteral("schnelle-umlaute"));
    QGuiApplication::setWindowIcon(
        QIcon::fromTheme(QStringLiteral("schnelle-umlaute-editor")));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;
    // loadFromModule is Qt 6.5+; fall back to a direct URL on older Qt
    // (Ubuntu 24.04 still ships Qt 6.4). The default resource prefix in
    // Qt 6.4 is /<URI>/ because QTP0001 doesn't exist yet.
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    engine.loadFromModule("SchnelleUmlaute", "Main");
#else
    engine.load(QUrl(QStringLiteral("qrc:/SchnelleUmlaute/Main.qml")));
#endif
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }
    return app.exec();
}
