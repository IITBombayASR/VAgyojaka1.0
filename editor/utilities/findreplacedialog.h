#pragma once

#include <QDialog>
#include <QPlainTextEdit>

namespace Ui {
    class FindReplaceDialog;
}

class FindReplaceDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FindReplaceDialog(QPlainTextEdit *parentEditor);
    ~FindReplaceDialog();

private slots:
    void updateFlags();
    void findPrevious();
    void findNext();
    void replace();
    void replaceAll();
signals:
    void message(const QString& text, int timeout = 2000);

private:
    QPlainTextEdit *m_Editor = nullptr;
    Ui::FindReplaceDialog *ui;
    QTextDocument::FindFlags flags;
};
