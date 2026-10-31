#include "renderer.h"

#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>

QString displayMathSource(const QString &formula) {
    static const QRegularExpression begin(
        R"(^\\begin\{(align|alignat|flalign|gather|multline|equation|eqnarray|displaymath)(\*?)\})");
    const auto match = begin.match(formula);
    if (!match.hasMatch())
        return "\\[\n" + formula + "\n\\]";
    const QString name = match.captured(1);
    if (name == "displaymath" || !match.captured(2).isEmpty())
        return formula;
    QString unnumbered = formula;
    unnumbered.replace("\\begin{" + name + "}", "\\begin{" + name + "*}");
    unnumbered.replace("\\end{" + name + "}", "\\end{" + name + "*}");
    return unnumbered;
}

namespace {
bool hasTextOutsideMath(const QString &source) {
    bool sawMath = false;
    QString text;
    for (int i = 0; i < source.size();) {
        if (source.mid(i, 2) == "$$") {
            const int end = source.indexOf("$$", i + 2);
            if (end < 0)
                return true;
            sawMath = true;
            i = end + 2;
        } else if (source.mid(i, 2) == "\\[" && (i == 0 || source[i - 1] != '\\')) {
            const int end = source.indexOf("\\]", i + 2);
            if (end < 0)
                return true;
            sawMath = true;
            i = end + 2;
        } else if (source[i] == '$' && (i == 0 || source[i - 1] != '\\')) {
            const int end = source.indexOf('$', i + 1);
            if (end < 0)
                return true;
            sawMath = true;
            i = end + 1;
        } else {
            text += source[i++];
        }
    }
    return sawMath && !text.trimmed().isEmpty();
}

QString escapeProse(const QString &text) {
    QString result;
    bool math = false;
    for (const QChar character : text) {
        if (character == '$')
            math = !math;
        else if (!math && (character == '%' || character == '&' || character == '#' ||
                           character == '_'))
            result += '\\';
        result += character;
    }
    return result;
}

QString mixedTextBody(const QString &source) {
    QString body;
    bool inList = false;
    bool inDisplay = false;
    const auto endList = [&] {
        if (inList) {
            body += "\\end{itemize}\n";
            inList = false;
        }
    };
    const QStringList lines = source.split('\n');
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (inDisplay) {
            body += line + "\n";
            if (trimmed.endsWith("$$") || trimmed.endsWith("\\]"))
                inDisplay = false;
            continue;
        }
        if (trimmed.startsWith("$$") || trimmed.startsWith("\\[")) {
            endList();
            body += line + "\n";
            const QString close = trimmed.startsWith("$$") ? "$$" : "\\]";
            if (trimmed.size() <= 2 + close.size() || !trimmed.endsWith(close))
                inDisplay = true;
            continue;
        }
        if (trimmed.isEmpty()) {
            endList();
            body += "\n";
            continue;
        }
        if (trimmed.startsWith("- ")) {
            if (!inList) {
                body += "\\begin{itemize}\n";
                inList = true;
            }
            body += "\\item " + escapeProse(trimmed.mid(2)) + "\n";
            continue;
        }
        endList();
        body += escapeProse(trimmed) + "\\par\n";
    }
    endList();
    return body;
}
}

QString latexBody(const QString &source) {
    const QString text = source.trimmed();
    if (hasTextOutsideMath(text))
        return "\\begin{minipage}[t]{6.5in}\n"
               "\\setlength{\\parindent}{0pt}\n"
               "\\setlength{\\parskip}{0.5\\baselineskip}\n"
               "\\raggedright\n" +
               mixedTextBody(text) + "\\end{minipage}";
    QString formula = text;
    if (formula.startsWith("$$") && formula.endsWith("$$") && formula.size() >= 4)
        formula = formula.mid(2, formula.size() - 4).trimmed();
    else if (formula.startsWith("\\[") && formula.endsWith("\\]"))
        formula = formula.mid(2, formula.size() - 4);
    else if (formula.startsWith("$") && formula.endsWith("$") && formula.size() > 2)
        formula = formula.mid(1, formula.size() - 2);
    return displayMathSource(formula);
}

Renderer::Renderer(QObject *parent) : QObject(parent) {
    process.setProcessChannelMode(QProcess::MergedChannels);
    connect(&process, &QProcess::finished, this, &Renderer::finished);
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (!cancelled && error == QProcess::FailedToStart)
            fail("Could not start " + process.program() + ": " + process.errorString());
    });
}

void Renderer::cancel() {
    cancelled = true;
    if (busy()) {
        process.kill();
        process.waitForFinished();
    }
    directory.reset();
}

void Renderer::render(const QString &source) {
    cancel();
    cancelled = false;
    for (const QString &program : {QString("latex"), QString("dvisvgm")}) {
        if (QStandardPaths::findExecutable(program).isEmpty()) {
            fail("Missing " + program + ". Install it and restart LaTeX Pad.");
            return;
        }
    }
    directory = std::make_unique<QTemporaryDir>();
    if (!directory->isValid()) {
        fail("Could not create a temporary rendering directory.");
        return;
    }
    QFile sourceFile(directory->filePath("equation.tex"));
    if (!sourceFile.open(QIODevice::WriteOnly)) {
        fail(sourceFile.errorString());
        return;
    }
    const QString document =
        "\\documentclass[12pt]{article}\n"
        "\\usepackage[paperwidth=100in,paperheight=100in,margin=0pt]{geometry}\n"
        "\\usepackage{amsmath,amssymb,bm}\n"
        "\\pagestyle{empty}\n"
        "\\begin{document}\n" +
        latexBody(source) +
        "\n\\end{document}\n";
    sourceFile.write(document.toUtf8());
    sourceFile.close();
    stage = Latex;
    process.setWorkingDirectory(directory->path());
    process.start("latex", {"-no-shell-escape", "-interaction=nonstopmode",
                            "-halt-on-error", "-file-line-error", "equation.tex"});
}

void Renderer::fail(const QString &message) {
    directory.reset();
    emit failed(message);
}

void Renderer::finished(int exitCode, QProcess::ExitStatus status) {
    if (cancelled)
        return;
    const QString output = QString::fromLocal8Bit(process.readAll());
    if (exitCode != 0 || status != QProcess::NormalExit) {
        QString message = output.trimmed();
        if (stage == Latex) {
            const QString transcript = message.section("Transcript written", 0, 0);
            const auto lineError = QRegularExpression(R"(equation\.tex:\d+:.*)")
                .match(transcript);
            const auto bangError = QRegularExpression(R"((?:^|\n)!.*)")
                .match(transcript);
            const auto match = lineError.hasMatch() ? lineError : bangError;
            if (match.hasMatch())
                message = match.captured().section("<inserted text>", 0, 0).trimmed();
            else if (transcript.contains("Emergency stop"))
                message = transcript.section("! ", -1).section('\n', 0, 0).trimmed();
        }
        fail(message.isEmpty() ? "Rendering process failed." : message);
        return;
    }
    if (stage == Latex) {
        stage = Svg;
        process.start("dvisvgm", {"--no-fonts", "--exact", "--bbox=min",
                                 "--output=equation.svg", "equation.dvi"});
        return;
    }
    QFile result(directory->filePath("equation.svg"));
    if (!result.open(QIODevice::ReadOnly)) {
        fail("The expression produced no visible output.");
        return;
    }
    const QByteArray svg = result.readAll();
    directory.reset();
    emit ready(svg);
}
