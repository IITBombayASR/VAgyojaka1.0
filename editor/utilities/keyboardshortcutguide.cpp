#include "keyboardshortcutguide.h"

#include <QHBoxLayout>
#include <QKeySequence>
#include <QHeaderView>

KeyboardShortcutGuide::KeyboardShortcutGuide(QWidget *parent)
    : QDialog(parent)
{
    QDialog::setWindowTitle("Keyboard Shortcuts Guide");

    int nWidth = 600;
    int nHeight = 800;
    if (parent != NULL)
        setGeometry(parent->x() + parent->width()/2 - nWidth/2,
            parent->y() + parent->height()/2 - nHeight/2,
            nWidth, nHeight);
    else
        resize(nWidth, nHeight);

    auto layout = new QHBoxLayout(this);
    m_shortcutView = new QTreeWidget(this);
    m_shortcutView->setColumnCount(2);
    m_shortcutView->setHeaderLabels(QStringList({"Type", "Description"}));
    m_shortcutView->header()->resizeSections(QHeaderView::Interactive);


    layout->addWidget(m_shortcutView);
    setLayout(layout);

    fillDialog();
}

void KeyboardShortcutGuide::fillDialog() {

    auto app = new QTreeWidgetItem({"Application"});

    QStringList closeApp({"Close Application", QKeySequence(Qt::ALT+Qt::Key_F4).toString()});
    app->addChild(new QTreeWidgetItem(closeApp));

    auto editing = new QTreeWidgetItem({"Editing"});
    QStringList undo({"Undo", QKeySequence(Qt::CTRL+Qt::Key_Z).toString()});
    QStringList redo({"Redo", QKeySequence(Qt::CTRL+Qt::Key_Y).toString()});
    QStringList cut({"Cut", QKeySequence(Qt::CTRL+Qt::Key_X).toString()});
    QStringList copy({"Copy", QKeySequence(Qt::CTRL+Qt::Key_C).toString()});
    QStringList paste({"Paste", QKeySequence(Qt::CTRL+Qt::Key_V).toString()});
    QStringList findReplace({"Find / Replace", QKeySequence(Qt::CTRL+Qt::Key_F).toString()});
    QStringList zoomIn({"Increase Font Size", QKeySequence(Qt::CTRL+Qt::Key_Equal).toString()});
    QStringList zoomOut({"Decrease Font Size", QKeySequence(Qt::CTRL+Qt::Key_Minus).toString()});
    QStringList saveTranscript({"Save Transcript", QKeySequence(Qt::CTRL+Qt::Key_S).toString()});
    QStringList splitLine({"Split Line", QKeySequence(Qt::CTRL+Qt::Key_Semicolon).toString()});
    QStringList jumpToHighlightedLine({"Jump to Highlighted Line", QKeySequence(Qt::CTRL+Qt::Key_J).toString()});
    QStringList mergeUp({"Merge Up", QKeySequence(Qt::CTRL+Qt::Key_Up).toString()});
    QStringList mergeDown({"Merge Down", QKeySequence(Qt::CTRL+Qt::Key_Down).toString()});
    QStringList toggleWordEditor({"Toggle Word Editor", QKeySequence(Qt::CTRL+Qt::Key_W).toString()});
    QStringList changeSpeaker({"Change Speaker", QKeySequence(Qt::CTRL+Qt::Key_R).toString()});
    QStringList propagateTime({"Propagate Time", QKeySequence(Qt::CTRL+Qt::Key_T).toString()});
    QStringList editTags({"Edit Tags", QKeySequence(Qt::CTRL+Qt::Key_Apostrophe).toString()});

    editing->addChild(new QTreeWidgetItem(undo));
    editing->addChild(new QTreeWidgetItem(redo));
    editing->addChild(new QTreeWidgetItem(cut));
    editing->addChild(new QTreeWidgetItem(copy));
    editing->addChild(new QTreeWidgetItem(paste));
    editing->addChild(new QTreeWidgetItem(findReplace));
    editing->addChild(new QTreeWidgetItem(zoomIn));
    editing->addChild(new QTreeWidgetItem(zoomOut));
    editing->addChild(new QTreeWidgetItem(saveTranscript));
    editing->addChild(new QTreeWidgetItem(splitLine));
    editing->addChild(new QTreeWidgetItem(jumpToHighlightedLine));
    editing->addChild(new QTreeWidgetItem(mergeUp));
    editing->addChild(new QTreeWidgetItem(mergeDown));
    editing->addChild(new QTreeWidgetItem(toggleWordEditor));
    editing->addChild(new QTreeWidgetItem(changeSpeaker));
    editing->addChild(new QTreeWidgetItem(propagateTime));
    editing->addChild(new QTreeWidgetItem(editTags));

    auto insertTimeStamp = new QTreeWidgetItem({"Insert Player timestamp in active editor", QKeySequence(Qt::CTRL + Qt::Key_I).toString()});

    auto jumpToPlayerTime = new QTreeWidgetItem({"Jump To Player"});
    QStringList speakerJumpUp({"Jump to line above with same speaker", QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_Up).toString()});
    QStringList speakerJumpDown({"Jump to line down with same speaker", QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_Down).toString()});
    QStringList wordJumpLeft({"Jump to word on left", QKeySequence(Qt::ALT+Qt::Key_Left).toString()});
    QStringList wordJumpRight({"Jump to word on right", QKeySequence(Qt::ALT+Qt::Key_Right).toString()});
    QStringList blockJumpUp({"Jump one block up", QKeySequence(Qt::ALT+Qt::Key_Up).toString()});
    QStringList blockJumpDown({"Jump one block down", QKeySequence(Qt::ALT+Qt::Key_Down).toString()});

    jumpToPlayerTime->addChild(new QTreeWidgetItem(speakerJumpUp));
    jumpToPlayerTime->addChild(new QTreeWidgetItem(speakerJumpDown));
    jumpToPlayerTime->addChild(new QTreeWidgetItem(wordJumpLeft));
    jumpToPlayerTime->addChild(new QTreeWidgetItem(wordJumpRight));
    jumpToPlayerTime->addChild(new QTreeWidgetItem(blockJumpUp));
    jumpToPlayerTime->addChild(new QTreeWidgetItem(blockJumpDown));

    auto mediaPlayer = new QTreeWidgetItem({"Media Player"});
    QStringList playPause({"Play / Pause", QKeySequence(Qt::CTRL+Qt::Key_Space).toString()});
    QStringList seekForward({"Seek Forward", QKeySequence(Qt::CTRL+Qt::Key_Period).toString()});
    QStringList seekBackward({"Seek Backward", QKeySequence(Qt::CTRL+Qt::Key_Comma).toString()});

    mediaPlayer->addChild(new QTreeWidgetItem(playPause));
    mediaPlayer->addChild(new QTreeWidgetItem(seekForward));
    mediaPlayer->addChild(new QTreeWidgetItem(seekBackward));

    m_shortcutView->insertTopLevelItem(0, app);
    m_shortcutView->insertTopLevelItem(1, insertTimeStamp);
    m_shortcutView->insertTopLevelItem(2, jumpToPlayerTime);
    m_shortcutView->insertTopLevelItem(3, editing);
    m_shortcutView->insertTopLevelItem(4, mediaPlayer);

    m_shortcutView->resizeColumnToContents(1);
    m_shortcutView->expandAll();
}


