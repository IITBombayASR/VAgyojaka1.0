#pragma once

#include <QMainWindow>
#include "mediaplayer/mediaplayer.h"
#include "editor/texteditor.h"


QT_BEGIN_NAMESPACE
namespace Ui { class Tool; }
QT_END_NAMESPACE

class Tool final : public QMainWindow
{
    Q_OBJECT

public:
    explicit Tool(QWidget *parent = nullptr);
    ~Tool() final;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void handleMediaPlayerError();
    void createKeyboardShortcutGuide();
    void changeFont();
    void changeFontSize(int change);
    void transliterationSelected(QAction* action);

private:
    void setFontForElements();
    void setTransliterationLangCodes();

    MediaPlayer *player = nullptr;
    Ui::Tool *ui;
    QFont font;
    QMap<QString, QString> m_transliterationLang;
};
