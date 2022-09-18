#pragma once

#include <QDialog>

namespace Ui {
    class TagSelectionDialog;
}

class TagSelectionDialog: public QDialog
{
    Q_OBJECT

public:
    explicit TagSelectionDialog(QWidget* parent = nullptr);
    ~TagSelectionDialog();
    QStringList tagList() const;

public slots:
    void markExistingTags(const QStringList& existingTagsList);

private:
    Ui::TagSelectionDialog* ui;
    QStringList m_languages, m_languageCodes;
};
