#include "window.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QPlainTextEdit>
#include <QTimer>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    Window window;
    QString source = R"(\widehat F_g(t)=\frac{1}{n_g}\sum_{i=1}^{n_g}\mathbf{1}(s_{gi}\leq t))";
    if (argc > 1) {
        QFile file(argv[1]);
        if (file.open(QIODevice::ReadOnly))
            source = QString::fromUtf8(file.readAll());
    }
    const QString output = argc > 2 ? QString(argv[2]) : QDir::currentPath();
    window.sourceEditor()->setPlainText(source);
    window.togglePreview();
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&] {
        if (!window.previewWidget()->valid())
            return;
        timer.stop();
        const bool svg = window.exportFile(output + "/equation.svg", "svg");
        const bool png = window.exportFile(output + "/equation.png", "png");
        const bool pdf = window.exportFile(output + "/equation.pdf", "pdf");
        qInfo() << "export" << svg << png << pdf;
        app.exit(svg && png && pdf ? 0 : 1);
    });
    timer.start(50);
    QTimer::singleShot(15000, &app, [&] { qInfo() << "timeout"; app.exit(2); });
    return app.exec();
}
