#include "window.h"

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QComboBox>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

namespace {
const QString equation =
    R"(\widehat F_g(t)=\frac{1}{n_g}\sum_{i=1}^{n_g}\mathbf{1}(s_{gi}\leq t))";

void waitForRender(Window &window) {
    QTRY_VERIFY_WITH_TIMEOUT(window.previewWidget()->valid(), 15000);
}
}

class AppTest : public QObject {
    Q_OBJECT
private slots:
    void init() {
        QSettings().clear();
    }

    void rendersAlignedEquation() {
        const auto startup = QApplication::palette();
        Window window;
        QApplication::setPalette(startup);
        auto *toggle = window.findChild<QToolButton *>("toggleButton");
        QVERIFY(toggle);
        window.sourceEditor()->setPlainText(
            R"(\begin{aligned} a^2+b^2&=c^2\\ \int_0^1 x^2\,dx&=\frac13\end{aligned})");
        QTest::mouseClick(toggle, Qt::LeftButton);
        waitForRender(window);
        QVERIFY(window.isPreview());
        QCOMPARE(toggle->text(), QString("Edit"));
        QVERIFY(window.previewWidget()->equationSize().width() > 100);
    }

    void rendersAlignEnvironment() {
        QTemporaryDir directory;
        Window window;
        window.sourceEditor()->setPlainText(
            "\\begin{align}\n"
            "\\widehat F_g(t)&=\\frac{1}{n_g}\\sum_{i=1}^{n_g}\\mathbf 1(s_{gi}\\leq t) \\\\\n"
            "\\widehat F_g(t)&=\\frac{1}{n_g}\\sum_{i=1}^{n_g}\\mathbf 3(s_{gi}\\leq t)\n"
            "\\end{align}");
        window.togglePreview();
        waitForRender(window);
        QVERIFY(window.isPreview());
        QVERIFY(window.previewWidget()->equationSize().height() > 100);
        QVERIFY2(window.previewWidget()->equationSize().width() < 400,
                 qPrintable(QString::number(window.previewWidget()->equationSize().width())));
        QVERIFY(window.exportFile(directory.filePath("align.pdf"), "pdf"));
    }

    void rendersMixedTextAndMath() {
        QTemporaryDir directory;
        Window window;
        window.sourceEditor()->setPlainText(R"(Geometric mean: multiplicative changes
Use it for growth factors, fold changes or ratios that accumulate by multiplication.
Suppose a quantity doubles during one period and halves during the next. The two factors are 2 and 0.5. Their average multiplicative factor is:
$$
G=\sqrt{2\times0.5}=1.
$$
That correctly represents no net change over the two periods. The arithmetic mean, 1.25, would incorrectly imply an average 25% increase per period.
The geometric mean is also used to summarize positive, skewed measurements on a logarithmic scale. It equals the exponential of the arithmetic mean of their logarithms.
Harmonic mean: rates over equal amounts of work
Suppose an analyser processes:
- A first batch of 60 samples at 30 samples/hour, taking 2 hours.
- A second batch of 60 samples at 60 samples/hour, taking 1 hour.
The overall rate is:
$$
H=\frac{120\text{ samples}}{3\text{ hours}}=40\text{ samples/hour}.
$$
That is the harmonic mean of the two rates:
$$
H=\frac{2}{1/30+1/60}=40.
$$
The slower batch occupies more time. If instead you ran each rate for one hour, the overall rate would be the arithmetic mean, 45 samples/hour. The weighting matters.)");
        window.togglePreview();
        waitForRender(window);
        QVERIFY(window.isPreview());
        const QSizeF size = window.previewWidget()->equationSize();
        QVERIFY2(size.width() > 600 && size.width() < 700,
                 qPrintable(QString::number(size.width()) + "x" + QString::number(size.height())));
        QVERIFY(size.height() > 500);
        const QString body = latexBody(window.sourceEditor()->toPlainText());
        QVERIFY(body.contains("average 25\\% increase"));
        QVERIFY(body.contains("\\begin{itemize}"));
        QVERIFY(body.contains("\\item A first batch"));
        QCOMPARE(latexBody("$$ x^2 $$"), QString("\\[\nx^2\n\\]"));
        QCOMPARE(latexBody("$x^2$"), QString("\\[\nx^2\n\\]"));
        QVERIFY(!latexBody("x^2").contains("minipage"));
        QVERIFY(latexBody("The value is $x^2$.").contains("minipage"));
        QVERIFY(!latexBody("\\begin{aligned}a&=b\\\\[2pt]c&=d\\end{aligned}")
                     .contains("minipage"));
        QVERIFY(window.exportFile(directory.filePath("notes.pdf"), "pdf"));
        QVERIFY(window.exportFile(directory.filePath("notes.png"), "png"));
    }

    void invalidMathKeepsSourceAndReturnsToEditor() {
        Window window;
        window.sourceEditor()->setPlainText(R"(\frac{1}{)");
        window.togglePreview();
        QTRY_VERIFY_WITH_TIMEOUT(!window.sourceEditor()->isReadOnly(), 15000);
        QVERIFY(!window.isPreview());
        auto *error = window.findChild<QLabel *>("renderError");
        QVERIFY(error);
        QVERIFY(error->isVisibleTo(&window));
        QVERIFY2(error->text().contains("File ended while scanning"),
                 qPrintable(error->text()));
        QVERIFY(!error->text().contains("Document Class"));
        QCOMPARE(window.sourceEditor()->toPlainText(), QString(R"(\frac{1}{)"));
        QVERIFY(!window.previewWidget()->valid());
    }

    void emptySourceDoesNotRender() {
        Window window;
        window.sourceEditor()->clear();
        window.togglePreview();
        QVERIFY(!window.isPreview());
        QVERIFY(!window.findChild<QToolButton *>("toggleButton")->text().isEmpty());
    }

    void recoversAfterFixingSyntaxError() {
        Window window;
        window.sourceEditor()->setPlainText("\\frac{1}{");
        window.togglePreview();
        QTRY_VERIFY_WITH_TIMEOUT(!window.sourceEditor()->isReadOnly(), 15000);
        window.sourceEditor()->setPlainText(equation);
        window.togglePreview();
        waitForRender(window);
        QVERIFY(window.isPreview());
        QVERIFY(!window.findChild<QLabel *>("renderError")->isVisibleTo(&window));
    }

    void displayMathDelimitersAreAccepted() {
        Window window;
        window.sourceEditor()->setPlainText("\\[ x^2 + y^2 \\]");
        window.togglePreview();
        waitForRender(window);
        QVERIFY(window.isPreview());
    }

    void editingInvalidatesExports() {
        Window window;
        window.sourceEditor()->setPlainText(equation);
        window.togglePreview();
        waitForRender(window);
        auto *svg = window.findChild<QAction *>("exportSvg");
        QVERIFY(svg);
        QVERIFY(svg->isEnabled());
        window.togglePreview();
        window.sourceEditor()->setPlainText(equation + " changed");
        QVERIFY(!svg->isEnabled());
        QVERIFY(!window.findChild<QToolButton *>("copyImage")->isEnabled());
    }

    void tallEquationCanScroll() {
        Window window;
        window.resize(660, 300);
        window.show();
        QStringList rows;
        rows << "\\begin{aligned}";
        for (int i = 0; i < 40; ++i)
            rows << "x_" + QString::number(i) + " &= " + QString::number(i) + " \\\\";
        rows << "\\end{aligned}";
        window.sourceEditor()->setPlainText(rows.join('\n'));
        window.togglePreview();
        waitForRender(window);
        auto *scroll = window.findChild<QScrollArea *>("previewScroll");
        QVERIFY(scroll);
        QVERIFY2(window.previewWidget()->minimumSize().height() > scroll->viewport()->height(),
                 qPrintable(QString("content %1 equation %2 viewport %3 rows %4")
                     .arg(window.previewWidget()->minimumSize().height())
                     .arg(window.previewWidget()->equationSize().height())
                     .arg(scroll->viewport()->height())
                     .arg(window.sourceEditor()->toPlainText().count('\n'))));
        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
    }

    void wideEquationCanScroll() {
        Window window;
        window.resize(660, 300);
        window.show();
        QStringList terms;
        for (int i = 0; i < 30; ++i)
            terms << "x_" + QString::number(i);
        window.sourceEditor()->setPlainText(terms.join(" + "));
        window.togglePreview();
        waitForRender(window);
        auto *scroll = window.findChild<QScrollArea *>("previewScroll");
        QVERIFY(scroll->horizontalScrollBar()->maximum() > 0);
    }

    void cancelRenderingReturnsToEditor() {
        Window window;
        window.sourceEditor()->setPlainText(equation);
        window.togglePreview();
        QVERIFY(window.sourceEditor()->isReadOnly());
        window.togglePreview();
        QVERIFY(!window.sourceEditor()->isReadOnly());
        QVERIFY(!window.isPreview());
        window.sourceEditor()->setPlainText("x");
        window.togglePreview();
        waitForRender(window);
        QCOMPARE(window.sourceEditor()->toPlainText(), QString("x"));
    }

    void toggleBackToSource() {
        Window window;
        window.sourceEditor()->setPlainText(equation);
        window.togglePreview();
        waitForRender(window);
        window.togglePreview();
        QVERIFY(!window.isPreview());
        QCOMPARE(window.sourceEditor()->toPlainText(), QString(equation));
    }

    void exportSvgPngAndPdf() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Window window;
        window.sourceEditor()->setPlainText(equation);
        window.togglePreview();
        waitForRender(window);
        const QString base = directory.filePath("output");
        QVERIFY(window.exportFile(base + ".svg", "svg"));
        QVERIFY(window.exportFile(base + ".png", "png"));
        QVERIFY(window.exportFile(base + ".pdf", "pdf"));
        QVERIFY2(window.exportFile(base + ".pdf", "pdf"), "overwriting an existing PDF failed");
        for (const QString &suffix : {".svg", ".png", ".pdf"}) {
            QFile file(base + suffix);
            QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));
            QVERIFY2(file.size() > 1000, qPrintable(suffix + " is suspiciously small"));
        }
        QImage png(base + ".png");
        QVERIFY(!png.isNull());
        QVERIFY(png.format() == QImage::Format_ARGB32 ||
                png.format() == QImage::Format_ARGB32_Premultiplied);
        QFile svg(base + ".svg");
        QVERIFY(svg.open(QIODevice::ReadOnly));
        const QByteArray content = svg.readAll();
        QVERIFY(content.startsWith("<?xml"));
        QVERIFY(!content.contains("#0000ff"));
        QFile pdf(base + ".pdf");
        QVERIFY(pdf.open(QIODevice::ReadOnly));
        QCOMPARE(pdf.read(5), QByteArray("%PDF-"));
    }

    void pngScaleAndTransparency() {
        QTemporaryDir directory;
        Window window;
        window.sourceEditor()->setPlainText(equation);
        window.togglePreview();
        waitForRender(window);
        auto *scale = window.findChild<QComboBox *>("exportScale");
        QVERIFY(scale);
        scale->setCurrentIndex(scale->findData(1));
        QVERIFY(window.exportFile(directory.filePath("one.png"), "png"));
        scale->setCurrentIndex(scale->findData(4));
        QVERIFY(window.exportFile(directory.filePath("four.png"), "png"));
        QImage one(directory.filePath("one.png"));
        QImage four(directory.filePath("four.png"));
        QVERIFY(qAbs(four.width() - one.width() * 4) <= 1);
        QVERIFY(qAbs(four.height() - one.height() * 4) <= 1);
        QVERIFY(one.width() > 100);
        QCOMPARE(one.pixelColor(2, 2), QColor(Qt::white));
        auto *transparent = window.findChild<QAction *>("transparent");
        QVERIFY(transparent);
        transparent->setChecked(true);
        QVERIFY(window.exportFile(directory.filePath("clear.png"), "png"));
        QImage clear(directory.filePath("clear.png"));
        QCOMPARE(clear.pixelColor(2, 2).alpha(), 0);
    }

    void copyImageToClipboard() {
        Window window;
        window.sourceEditor()->setPlainText(equation);
        window.togglePreview();
        waitForRender(window);
        window.copyImage();
        const QImage image = QApplication::clipboard()->image();
        QVERIFY(!image.isNull());
        auto *scale = window.findChild<QComboBox *>("exportScale");
        QVERIFY(scale);
        const int factor = scale->currentData().toInt();
        QVERIFY2(image.width() > 100 * factor, qPrintable(QString::number(image.width())));
        QVERIFY(QApplication::clipboard()->mimeData()->hasFormat("image/png") ||
                QApplication::clipboard()->mimeData()->hasImage());
    }

    void windowCanShrinkToCompactSize() {
        Window window;
        window.show();
        window.resize(420, 220);
        QApplication::processEvents();
        QCOMPARE(window.size(), QSize(420, 220));
        QVERIFY(window.minimumSizeHint().width() <= 420);
        QVERIFY(window.minimumSizeHint().height() <= 220);
    }

    void fileSaveAndScratchpadRestore() {
        QTemporaryDir directory;
        const QString path = directory.filePath("notes.tex");
        {
            Window window;
            window.sourceEditor()->setPlainText("a+b");
            QVERIFY(window.saveFile(path));
            QCOMPARE(window.windowTitle(), QString("notes.tex · LaTeX Pad"));
        }
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("a+b"));
        {
            Window window;
            QCOMPARE(window.sourceEditor()->toPlainText(), QString("a+b"));
            QCOMPARE(window.windowTitle(), QString("notes.tex · LaTeX Pad"));
            window.sourceEditor()->setPlainText("scratch");
        }
        QSettings settings;
        QCOMPARE(settings.value("source").toString(), QString("scratch"));
        settings.setValue("filePath", QString());
        Window next;
        QCOMPARE(next.sourceEditor()->toPlainText(), QString("scratch"));
        QCOMPARE(next.windowTitle(), QString("Scratchpad · LaTeX Pad"));
    }
};

QTEST_MAIN(AppTest)
#include "app_test.moc"
