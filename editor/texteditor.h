#pragma once

#include "utilities/findreplacedialog.h"

#include <QPlainTextEdit>

class LineNumberArea;

class TextEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit TextEditor(QWidget *parent = nullptr);
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();

    void setLineNumberAreaFont(const QFont& font)
    {
        lineNumberArea->setFont(font);
    }

public slots:
    void findReplace();

signals:
    void message(const QString& text, int timeout = 5000);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    QWidget *lineNumberArea;
    FindReplaceDialog *m_findReplace = nullptr;
};

class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(TextEditor *parentEditor) : QWidget(parentEditor), m_editor(parentEditor)
    {}

    QSize sizeHint() const override
    {
        return QSize(m_editor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        m_editor->lineNumberAreaPaintEvent(event);
    }

private:
    TextEditor *m_editor;
};
