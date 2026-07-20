#include <QCommandLineParser>
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
    QGuiApplication::setApplicationVersion(
        QStringLiteral(SCHNELLE_UMLAUTE_VERSION));

    // Handle --version / --help before any window or single-instance work, so
    // both print and exit immediately regardless of a running editor instance.
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Schnelle Umlaute profile editor"));
    parser.addHelpOption();    // -h / --help
    parser.addVersionOption(); // -v / --version
    parser.process(app);

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
    engine.rootContext()->setContextProperty(
        QStringLiteral("appVersion"),
        QCoreApplication::applicationVersion());
    // loadFromModule is Qt 6.5+; fall back to a direct URL on older Qt
    // (Ubuntu 24.04 still ships Qt 6.4). Loaded by URL, Main.qml is not
    // associated with its module, so `import SchnelleUmlaute` (which provides
    // the Theme singleton) must find the embedded qmldir on an import path.
    // The build tree exposes it via the filesystem, but the installed binary
    // relies purely on the qrc, so add /qt/qml (the pinned RESOURCE_PREFIX)
    // explicitly. Without this the singleton is unresolved once installed and
    // every Theme.* binding is undefined.
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    engine.loadFromModule("SchnelleUmlaute", "Main");
#else
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/SchnelleUmlaute/Main.qml")));
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
