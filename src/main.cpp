#include "window.h"
#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setOrganizationName("LatexPad");
    app.setApplicationName("LaTeX Pad");
    app.setDesktopFileName("latex-pad");
    QCommandLineParser parser;
    parser.setApplicationDescription("A local LaTeX math scratchpad");
    parser.addHelpOption();
    parser.addPositionalArgument("file", "LaTeX snippet to open", "[file]");
    parser.process(app);
    Window window(parser.positionalArguments().value(0));
    window.show();
    return app.exec();
}
