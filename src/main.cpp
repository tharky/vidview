#include "MainWindow.h"

#include <QApplication>
#include <QSurfaceFormat>

int main(
    int argc,
    char* argv[]
) {
    /*
        VidView's renderer uses OpenGL 3.3
        core profile.

        Set this BEFORE QApplication exists.
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

    /*
        Ask the driver to synchronize buffer
        swaps with the display.
    */
    format.setSwapInterval(1);

    QSurfaceFormat::setDefaultFormat(
        format
    );

    QApplication app(
        argc,
        argv
    );

    MainWindow window;

    window.show();

    return app.exec();
}