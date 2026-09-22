#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>
#include <QSurfaceFormat>
#include <QTimer>

#ifndef VIDVIEW_VERSION
#define VIDVIEW_VERSION "dev"
#endif

int main(
    int argc,
    char* argv[]
) {
    /*
        Configure OpenGL before QApplication
        creates any graphics resources.
    */
    QSurfaceFormat format;

    format.setRenderableType(
        QSurfaceFormat::OpenGL
    );

    format.setVersion(
        3,
        3
    );

    format.setProfile(
        QSurfaceFormat::CoreProfile
    );

    format.setSwapInterval(
        1
    );

    QSurfaceFormat::setDefaultFormat(
        format
    );

    QApplication app(
        argc,
        argv
    );

    QCoreApplication::setApplicationName(
        "VidView"
    );

    QCoreApplication::setApplicationVersion(
        VIDVIEW_VERSION
    );

    QCoreApplication::setOrganizationName(
        "VidView"
    );

    QApplication::setWindowIcon(
        QIcon(
            ":/VidView.ico"
        )
    );

    /*
        Proper CLI parser gives us:
          VidView.exe video.mp4
          VidView.exe --version
          VidView.exe --help
          VidView.exe --smoke-test
    */
    QCommandLineParser parser;

    parser.setApplicationDescription(
        "Fast native video inspection and frame analysis."
    );

    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption smokeTestOption(
        "smoke-test",
        "Verify that VidView and its runtime dependencies can start."
    );

    parser.addOption(
        smokeTestOption
    );

    parser.addPositionalArgument(
        "video",
        "Video file to open."
    );

    parser.process(
        app
    );

    if (
        parser.isSet(
            smokeTestOption
        )
    ) {
        return 0;
    }

    MainWindow window;

    window.show();

    const QStringList arguments =
        parser.positionalArguments();

    if (
        !arguments.isEmpty()
    ) {
        QFileInfo fileInfo(
            arguments.first()
        );

        QString path =
            fileInfo
                .absoluteFilePath();

        /*
            Let the Qt event loop start before
            queueing decoder work.
        */
        QTimer::singleShot(
            0,
            &window,
            [
                &window,
                path
            ]() {
                QFileInfo file(
                    path
                );

                if (
                    !file.exists() ||
                    !file.isFile()
                ) {
                    QMessageBox::warning(
                        &window,
                        "File not found",
                        QString(
                            "VidView could not find:\n%1"
                        ).arg(path)
                    );

                    return;
                }

                window.openInitialFile(
                    path
                );
            }
        );
    }

    return app.exec();
}