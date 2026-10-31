#pragma once

#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <memory>

QString latexBody(const QString &source);

class Renderer : public QObject {
    Q_OBJECT
public:
    explicit Renderer(QObject *parent = nullptr);
    void render(const QString &source);
    void cancel();
    bool busy() const { return process.state() != QProcess::NotRunning; }

signals:
    void ready(const QByteArray &svg);
    void failed(const QString &message);

private:
    void finished(int exitCode, QProcess::ExitStatus status);
    void fail(const QString &message);
    void start(const QString &program, const QStringList &arguments);
    QProcess process;
    std::unique_ptr<QTemporaryDir> directory;
    enum Stage { Latex, Svg } stage = Latex;
    bool cancelled = false;
};
