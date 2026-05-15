#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>

#include "ClickOutsideDefocus.h"
#include "EnvSetup.h"
#include "MappingListModel.h"
#include "SingleInstance.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(
        QStringLiteral("schnelle-umlaute-editor"));
    QGuiApplication::setOrganizationName(QStringLiteral("schnelle-umlaute"));
    QGuiApplication::setWindowIcon(
        QIcon::fromTheme(QStringLiteral("schnelle-umlaute-editor")));

    // Single-instance check before any UI work. Two editor windows
    // editing the same on-disk config would race on save and silently
    // overwrite each other; raise the existing window instead.
    if (!SingleInstance::acquireOrRaise()) {
        return 0;
    }

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    EnvSetup envSetup;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("envSetup"),
                                             &envSetup);
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
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (window) {
        window->installEventFilter(new ClickOutsideDefocus(window));
        SingleInstance::registerOnWindow(window);
    }
    return app.exec();
}
