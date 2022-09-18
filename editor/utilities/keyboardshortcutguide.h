
#pragma once

#include <QDialog>
#include <QTreeWidget>

class KeyboardShortcutGuide: public QDialog
{
    Q_OBJECT

public:
    explicit KeyboardShortcutGuide(QWidget* parent = nullptr);

private:
    void fillDialog();

    QTreeWidget* m_shortcutView;
};
