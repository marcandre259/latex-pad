#pragma once

#include "renderer.h"
#include <QMainWindow>
#include <QSvgRenderer>

class QAction;
class QCloseEvent;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QScrollArea;
class QStackedWidget;
class QTimer;
class QToolButton;

class Preview : public QWidget {
    Q_OBJECT
public:
    explicit Preview(QWidget *parent = nullptr);
    void setSvg(const QByteArray &data);
    void setZoom(int percent);
    QSizeF equationSize() const;
    QImage image(int scale, bool transparent) const;
    bool saveSvg(const QString &path, bool transparent);
    bool valid() const { return svg.isValid(); }

signals:
    void editRequested();

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    mutable QSvgRenderer svg;
    QByteArray svgData;
    int zoom = 150;
};

class Window : public QMainWindow {
    Q_OBJECT
public:
    explicit Window(const QString &path = {}, QWidget *parent = nullptr);
    ~Window() override;
    bool loadFile(const QString &path);
    bool saveFile(const QString &path);
    bool exportFile(const QString &path, const QString &format);
    QPlainTextEdit *sourceEditor() const { return editor; }
    Preview *previewWidget() const { return preview; }
    bool isPreview() const;

public slots:
    void togglePreview();
    void copyImage();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void showEditor();
    void updateTitle();
    void persist();
    void showError(const QString &message);
    bool confirmDiscard();
    bool save();
    void open();
    void exportDialog(const QString &format);
    void updateExportActions();

    Renderer renderer;
    QPlainTextEdit *editor;
    Preview *preview;
    QScrollArea *scroll;
    QStackedWidget *stack;
    QLabel *error;
    QToolButton *toggle;
    QComboBox *zoom;
    QComboBox *scale;
    QAction *transparent;
    QToolButton *copyButton;
    QAction *exportSvg;
    QAction *exportPng;
    QAction *exportPdf;
    QTimer *autosave;
    QString filePath;
    QString savedSource;
    QString renderedSource;
    bool rendering = false;
};
