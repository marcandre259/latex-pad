#include "window.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QDir>

namespace {
constexpr int padding = 12;
const QString sample = R"(\widehat F_g(t)=\frac{1}{n_g}\sum_{i=1}^{n_g}\mathbf{1}(s_{gi}\leq t))";

QByteArray paddedSvg(const QByteArray &data) {
    QString text = QString::fromUtf8(data);
    const QRegularExpression viewBox(R"(\bviewBox=['"]([^'"]*)['"])");
    const auto match = viewBox.match(text);
    if (!match.hasMatch())
        return data;
    const QStringList parts = match.captured(1).split(QRegularExpression(R"([\s,]+)"),
                                                      Qt::SkipEmptyParts);
    if (parts.size() != 4)
        return data;
    bool ok = true;
    double value[4];
    for (int i = 0; i < 4; ++i)
        value[i] = parts[i].toDouble(&ok);
    if (!ok)
        return data;
    const double margin = padding * 72.0 / 96.0;
    const QRectF box(value[0] - margin, value[1] - margin,
                     value[2] + 2 * margin, value[3] + 2 * margin);
    text.replace(match.capturedStart(), match.capturedLength(),
        QString("viewBox='%1 %2 %3 %4'").arg(box.x(), 0, 'g', 12)
            .arg(box.y(), 0, 'g', 12).arg(box.width(), 0, 'g', 12)
            .arg(box.height(), 0, 'g', 12));
    text.replace(QRegularExpression(R"(\bwidth=['"][^'"]*pt['"])"),
        QString("width='%1pt'").arg(box.width(), 0, 'g', 12));
    text.replace(QRegularExpression(R"(\bheight=['"][^'"]*pt['"])"),
        QString("height='%1pt'").arg(box.height(), 0, 'g', 12));
    return text.toUtf8();
}
}

Preview::Preview(QWidget *parent) : QWidget(parent) {
    setObjectName("preview");
    setCursor(Qt::IBeamCursor);
    setToolTip("Click to edit · Ctrl+Enter");
    setAutoFillBackground(true);
    QPalette colors = palette();
    colors.setColor(QPalette::Window, Qt::white);
    setPalette(colors);
}

void Preview::setSvg(const QByteArray &data) {
    svgData = paddedSvg(data);
    svg.load(svgData);
    setZoom(zoom);
}

QSizeF Preview::equationSize() const {
    return svg.viewBoxF().size() * (96.0 / 72.0);
}

void Preview::setZoom(int percent) {
    zoom = percent;
    const QSize size = (equationSize() * (zoom / 100.0)).toSize();
    setMinimumSize(size + QSize(2 * padding, 2 * padding));
    update();
}

void Preview::paintEvent(QPaintEvent *) {
    if (!svg.isValid())
        return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QSizeF size = equationSize() * (zoom / 100.0);
    painter.translate(qMax(0.0, (width() - size.width()) / 2),
                      qMax(0.0, (height() - size.height()) / 2));
    painter.scale(zoom / 100.0, zoom / 100.0);
    svg.render(&painter, QRectF(QPointF(0, 0), equationSize()));
}

void Preview::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton)
        emit editRequested();
}

QImage Preview::image(int scale, bool transparent) const {
    const QSizeF size = equationSize();
    QImage image((size * scale).toSize(), QImage::Format_ARGB32_Premultiplied);
    image.fill(transparent ? Qt::transparent : Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(scale, scale);
    svg.render(&painter, QRectF(QPointF(0, 0), size));
    return image;
}

bool Preview::saveSvg(const QString &path, bool transparent) {
    QString text = QString::fromUtf8(svgData);
    if (!transparent) {
        const int end = text.indexOf('>', text.indexOf("<svg"));
        const QRectF box = svg.viewBoxF();
        text.insert(end + 1, QString("\n<rect x='%1' y='%2' width='%3' height='%4' fill='white'/>")
            .arg(box.x(), 0, 'g', 12).arg(box.y(), 0, 'g', 12)
            .arg(box.width(), 0, 'g', 12).arg(box.height(), 0, 'g', 12));
    }
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(text.toUtf8()) != -1 && file.commit();
}

bool savePdf(const QString &source, const QString &path) {
    QTemporaryDir directory;
    if (!directory.isValid())
        return false;
    QFile tex(directory.filePath("formula.tex"));
    if (!tex.open(QIODevice::WriteOnly))
        return false;
    tex.write(("\\documentclass[12pt]{article}\n"
               "\\usepackage{amsmath,amssymb,bm}\n"
               "\\usepackage[active,tightpage]{preview}\n"
               "\\pagestyle{empty}\n"
               "\\begin{document}\n"
               "\\begin{preview}\n" +
               latexBody(source) +
               "\n\\end{preview}\n"
               "\\end{document}\n").toUtf8());
    tex.close();
    QProcess process;
    process.setWorkingDirectory(directory.path());
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start("pdflatex", {"-interaction=nonstopmode", "-halt-on-error",
                               "-no-shell-escape", "formula.tex"});
    if (!process.waitForFinished(30000) || process.exitCode() != 0)
        return false;
    QFile pdf(directory.filePath("formula.pdf"));
    QSaveFile output(path);
    if (!pdf.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))
        return false;
    const QByteArray data = pdf.readAll();
    return output.write(data) == data.size() && output.commit();
}

Window::Window(const QString &path, QWidget *parent)
    : QMainWindow(parent), renderer(this) {
    setObjectName("latexPadWindow");
    setWindowIcon(QIcon::fromTheme("latex-pad"));
    resize(660, 320);
    auto *toolbar = addToolBar("Controls");
    toolbar->setMovable(false);
    toolbar->setContextMenuPolicy(Qt::PreventContextMenu);

    auto *fileButton = new QToolButton;
    fileButton->setText("File");
    fileButton->setObjectName("fileButton");
    fileButton->setPopupMode(QToolButton::InstantPopup);
    auto *fileMenu = new QMenu(fileButton);
    auto *newAction = fileMenu->addAction("New", QKeySequence::New);
    connect(newAction, &QAction::triggered, this, [this] {
        if (!confirmDiscard()) return;
        showEditor();
        filePath.clear();
        savedSource.clear();
        editor->clear();
        updateTitle();
        persist();
    });
    auto *openAction = fileMenu->addAction("Open…", QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &Window::open);
    auto *saveAction = fileMenu->addAction("Save", QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, [this] { save(); });
    auto *saveAs = fileMenu->addAction("Save As…", QKeySequence::SaveAs);
    connect(saveAs, &QAction::triggered, this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, "Save LaTeX source", filePath,
            "LaTeX snippets (*.tex);;Text files (*.txt);;All files (*)");
        if (!path.isEmpty()) saveFile(path);
    });
    addActions({newAction, openAction, saveAction, saveAs});
    fileButton->setMenu(fileMenu);
    toolbar->addWidget(fileButton);

    toggle = new QToolButton;
    toggle->setText("Preview");
    toggle->setObjectName("toggleButton");
    toggle->setToolTip("Preview / edit · Ctrl+Enter");
    toolbar->addWidget(toggle);
    connect(toggle, &QToolButton::clicked, this, &Window::togglePreview);
    for (const auto key : {Qt::Key_Return, Qt::Key_Enter}) {
        auto *shortcut = new QShortcut(QKeySequence(Qt::CTRL | key), this);
        connect(shortcut, &QShortcut::activated, this, &Window::togglePreview);
    }

    toolbar->addSeparator();
    zoom = new QComboBox;
    zoom->setObjectName("zoom");
    zoom->setToolTip("Preview zoom. Does not change exports.");
    for (int percent : {75, 100, 150, 200, 300})
        zoom->addItem(QString::number(percent) + "%", percent);
    toolbar->addWidget(zoom);
    auto *spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);
    copyButton = new QToolButton;
    copyButton->setText("Copy Image");
    copyButton->setObjectName("copyImage");
    copyButton->setToolTip("Copy PNG at export scale · Ctrl+Shift+C");
    toolbar->addWidget(copyButton);
    connect(copyButton, &QToolButton::clicked, this, &Window::copyImage);
    auto *copyShortcut = new QShortcut(QKeySequence("Ctrl+Shift+C"), this);
    copyShortcut->setContext(Qt::ApplicationShortcut);
    connect(copyShortcut, &QShortcut::activated, this, &Window::copyImage);

    auto *exportButton = new QToolButton;
    exportButton->setText("Export");
    exportButton->setObjectName("exportButton");
    exportButton->setPopupMode(QToolButton::InstantPopup);
    auto *exportMenu = new QMenu(exportButton);
    exportSvg = exportMenu->addAction("Save SVG…", this, [this] { exportDialog("svg"); });
    exportSvg->setObjectName("exportSvg");
    exportPng = exportMenu->addAction("Save PNG…", this, [this] { exportDialog("png"); });
    exportPng->setObjectName("exportPng");
    exportPdf = exportMenu->addAction("Save PDF…", this, [this] { exportDialog("pdf"); });
    exportPdf->setObjectName("exportPdf");
    exportMenu->addSeparator();
    transparent = exportMenu->addAction("Transparent background (SVG / PNG)");
    transparent->setCheckable(true);
    transparent->setObjectName("transparent");
    exportButton->setMenu(exportMenu);
    toolbar->addWidget(exportButton);
    scale = new QComboBox;
    scale->setObjectName("exportScale");
    scale->setToolTip("PNG and clipboard resolution");
    for (int value : {1, 2, 3, 4, 6, 8})
        scale->addItem(QString::number(value) + "× PNG", value);
    toolbar->addWidget(scale);

    auto *central = new QWidget;
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    stack = new QStackedWidget;
    editor = new QPlainTextEdit;
    editor->setObjectName("source");
    editor->setPlaceholderText("Type math, or notes with $$...$$ display math. Ctrl+Enter to preview.");
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(12);
    editor->setFont(font);
    editor->setTabStopDistance(editor->fontMetrics().horizontalAdvance(' ') * 4);
    preview = new Preview;
    scroll = new QScrollArea;
    scroll->setObjectName("previewScroll");
    scroll->setWidget(preview);
    scroll->setWidgetResizable(true);
    scroll->setAlignment(Qt::AlignCenter);
    scroll->setFrameShape(QFrame::NoFrame);
    stack->addWidget(editor);
    stack->addWidget(scroll);
    layout->addWidget(stack);
    error = new QLabel;
    error->setObjectName("renderError");
    error->setTextFormat(Qt::PlainText);
    error->setWordWrap(true);
    error->setTextInteractionFlags(Qt::TextSelectableByMouse);
    error->setStyleSheet("QLabel { color: #b42318; background: #fff1ee; padding: 8px; }");
    error->setMaximumHeight(120);
    error->setMinimumHeight(0);
    error->hide();
    layout->addWidget(error);
    setCentralWidget(central);
    statusBar()->setSizeGripEnabled(true);

    autosave = new QTimer(this);
    autosave->setSingleShot(true);
    autosave->setInterval(300);
    connect(autosave, &QTimer::timeout, this, &Window::persist);
    connect(editor, &QPlainTextEdit::textChanged, this, [this] {
        error->hide();
        autosave->start();
        updateTitle();
        updateExportActions();
    });
    connect(preview, &Preview::editRequested, this, &Window::showEditor);
    connect(&renderer, &Renderer::ready, this, [this](const QByteArray &data) {
        rendering = false;
        preview->setSvg(data);
        if (!preview->valid()) {
            showError("Could not display the generated SVG.");
            return;
        }
        renderedSource = editor->toPlainText();
        stack->setCurrentWidget(scroll);
        toggle->setText("Edit");
        updateExportActions();
        statusBar()->showMessage("Click to edit · Ctrl+Enter");
    });
    connect(&renderer, &Renderer::failed, this, [this](const QString &message) {
        rendering = false;
        showEditor();
        showError(message);
    });
    connect(zoom, &QComboBox::currentIndexChanged, this, [this] {
        preview->setZoom(zoom->currentData().toInt());
        autosave->start();
    });
    connect(scale, &QComboBox::currentIndexChanged, this, [this] { autosave->start(); });
    connect(transparent, &QAction::toggled, this, [this] { autosave->start(); });

    QSettings settings;
    zoom->setCurrentIndex(qMax(0, zoom->findData(settings.value("zoom", 150).toInt())));
    scale->setCurrentIndex(qMax(0, scale->findData(settings.value("scale", 3).toInt())));
    transparent->setChecked(settings.value("transparent", false).toBool());
    if (!restoreGeometry(settings.value("geometry").toByteArray()))
        resize(660, 300);
    if (height() > 900 || width() > 1000)
        resize(qMin(width(), 760), 360);
    filePath = settings.value("filePath").toString();
    savedSource = settings.value("savedSource").toString();
    editor->setPlainText(settings.value("source", sample).toString());
    if (!path.isEmpty())
        loadFile(path);
    preview->setZoom(zoom->currentData().toInt());
    updateTitle();
    updateExportActions();
    statusBar()->showMessage("Ctrl+Enter to preview");
    editor->setFocus();
}

Window::~Window() {
    renderer.cancel();
    persist();
}

bool Window::isPreview() const {
    return stack->currentWidget() == scroll;
}

void Window::togglePreview() {
    if (isPreview() || rendering) {
        showEditor();
        return;
    }
    if (editor->toPlainText().trimmed().isEmpty()) {
        statusBar()->showMessage("Type an expression first.", 3000);
        return;
    }
    error->hide();
    if (preview->valid() && renderedSource == editor->toPlainText()) {
        stack->setCurrentWidget(scroll);
        toggle->setText("Edit");
        statusBar()->showMessage("Click to edit · Ctrl+Enter");
        return;
    }
    rendering = true;
    editor->setReadOnly(true);
    toggle->setText("Cancel");
    statusBar()->showMessage("Rendering locally…");
    renderer.render(editor->toPlainText());
}

void Window::showEditor() {
    renderer.cancel();
    rendering = false;
    editor->setReadOnly(false);
    stack->setCurrentWidget(editor);
    toggle->setText("Preview");
    editor->setFocus();
    statusBar()->showMessage("Ctrl+Enter to preview");
}

void Window::updateTitle() {
    const QString name = filePath.isEmpty() ? "Scratchpad" : QFileInfo(filePath).fileName();
    const bool modified = !filePath.isEmpty() && editor->toPlainText() != savedSource;
    setWindowTitle(name + (modified ? " *" : "") + " · LaTeX Pad");
}

void Window::persist() {
    QSettings settings;
    settings.setValue("source", editor->toPlainText());
    settings.setValue("filePath", filePath);
    settings.setValue("savedSource", savedSource);
    settings.setValue("geometry", saveGeometry());
    settings.setValue("zoom", zoom->currentData());
    settings.setValue("scale", scale->currentData());
    settings.setValue("transparent", transparent->isChecked());
    settings.sync();
}

void Window::showError(const QString &message) {
    error->setText(message);
    error->show();
    statusBar()->showMessage("Check the source and try again.");
}

bool Window::confirmDiscard() {
    if (editor->toPlainText().isEmpty() || editor->toPlainText() == savedSource)
        return true;
    const auto answer = QMessageBox::question(this, "Unsaved source",
        "Save your changes before replacing this source?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (answer == QMessageBox::Save)
        return save();
    return answer == QMessageBox::Discard;
}

bool Window::loadFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        showError("Could not open " + path + ": " + file.errorString());
        return false;
    }
    showEditor();
    filePath = QFileInfo(path).absoluteFilePath();
    savedSource = QString::fromUtf8(file.readAll());
    editor->setPlainText(savedSource);
    error->hide();
    updateTitle();
    persist();
    return true;
}

bool Window::saveFile(const QString &path) {
    QSaveFile file(path);
    const QByteArray data = editor->toPlainText().toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        showError("Could not save " + path + ": " + file.errorString());
        return false;
    }
    filePath = QFileInfo(path).absoluteFilePath();
    savedSource = editor->toPlainText();
    updateTitle();
    persist();
    statusBar()->showMessage("Saved " + QFileInfo(path).fileName(), 3000);
    return true;
}

bool Window::save() {
    QString path = filePath;
    if (path.isEmpty())
        path = QFileDialog::getSaveFileName(this, "Save LaTeX source", {},
            "LaTeX snippets (*.tex);;Text files (*.txt);;All files (*)");
    return !path.isEmpty() && saveFile(path);
}

void Window::open() {
    if (!confirmDiscard())
        return;
    const QString path = QFileDialog::getOpenFileName(this, "Open LaTeX source", filePath,
        "LaTeX snippets (*.tex *.txt);;All files (*)");
    if (!path.isEmpty())
        loadFile(path);
}

void Window::updateExportActions() {
    const bool current = preview->valid() && renderedSource == editor->toPlainText();
    copyButton->setEnabled(current);
    for (QAction *action : {exportSvg, exportPng, exportPdf})
        action->setEnabled(current);
}

void Window::copyImage() {
    if (!copyButton->isEnabled())
        return;
    QApplication::clipboard()->setImage(preview->image(scale->currentData().toInt(),
                                                     transparent->isChecked()));
    statusBar()->showMessage("Image copied", 3000);
}

bool Window::exportFile(const QString &path, const QString &format) {
    if (!exportSvg->isEnabled())
        return false;
    bool ok = false;
    if (format == "svg")
        ok = preview->saveSvg(path, transparent->isChecked());
    else if (format == "pdf") {
        if (QStandardPaths::findExecutable("pdflatex").isEmpty()) {
            showError("Missing pdflatex. Install TeX Live and restart LaTeX Pad.");
            return false;
        }
        ok = savePdf(editor->toPlainText(), path);
    }
    else if (format == "png") {
        QSaveFile file(path);
        ok = file.open(QIODevice::WriteOnly) &&
            preview->image(scale->currentData().toInt(), transparent->isChecked()).save(&file, "PNG") &&
            file.commit();
    }
    if (!ok)
        showError("Could not export to " + path + ". Check that the location is writable.");
    else
        statusBar()->showMessage("Exported " + QFileInfo(path).fileName(), 3000);
    return ok;
}

void Window::exportDialog(const QString &format) {
    const QString base = filePath.isEmpty() ? "equation" : QFileInfo(filePath).completeBaseName();
    const QString path = QFileDialog::getSaveFileName(this, "Export " + format.toUpper(),
        base + "." + format, format.toUpper() + " files (*." + format + ")");
    if (!path.isEmpty())
        exportFile(path, format);
}

void Window::closeEvent(QCloseEvent *event) {
    if (!filePath.isEmpty() && !confirmDiscard()) {
        event->ignore();
        return;
    }
    persist();
    renderer.cancel();
    event->accept();
}
