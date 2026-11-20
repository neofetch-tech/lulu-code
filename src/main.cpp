#include "app/Logger.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QIcon>
#include <QString>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QApplication::setOrganizationName(QStringLiteral("Lulu"));
    QApplication::setOrganizationDomain(QStringLiteral("lulu.dev"));
    QApplication::setApplicationName(QStringLiteral("Lulu Code"));
    QApplication::setApplicationDisplayName(QStringLiteral("Lulu Code"));
    QApplication::setApplicationVersion(QStringLiteral(LULU_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/logo.png")));
    QApplication::setDesktopFileName(QStringLiteral("lulu-code"));

    lulu::Logger::install();
    lulu::Theme::apply(app);

    lulu::MainWindow window;
    window.show();

    return QApplication::exec();
}
