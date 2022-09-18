#pragma once

#include "blockandword.h"
#include "texteditor.h"
#include "wordeditor.h"
#include "utilities/changespeakerdialog.h"
#include "utilities/timepropagationdialog.h"
#include "utilities/tagselectiondialog.h"

#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QCompleter>
#include <QAbstractItemModel>
#include <qcompleter.h>
#include <set>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>

class Highlighter;

class Editor : public TextEditor
{
    Q_OBJECT

public:
    explicit Editor(QWidget *parent = nullptr);

    void setWordEditor(WordEditor* wordEditor)
    {
        m_wordEditor = wordEditor;
        connect(m_wordEditor, &QTableWidget::itemChanged, this, &Editor::wordEditorChanged);
    }

    void setEditorFont(const QFont& font);

    QRegularExpression timeStampExp, speakerExp;

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

signals:
    void jumpToPlayer(const QTime& time);
    void refreshTagList(const QStringList& tagList);
    void replyCame();

public slots:
    void transcriptOpen();
    void transcriptSave();
    void transcriptSaveAs();
    void transcriptClose();
    void highlightTranscript(const QTime& elapsedTime);

    void showBlocksFromData();
    void jumpToHighlightedLine();
    void splitLine(const QTime& elapsedTime);
    void mergeUp();
    void mergeDown();
    void createChangeSpeakerDialog();
    void createTimePropagationDialog();
    void createTagSelectionDialog();
    void insertTimeStamp(const QTime& elapsedTime);
    void changeTranscriptLang();

    void speakerWiseJump(const QString& jumpDirection);
    void wordWiseJump(const QString& jumpDirection);
    void blockWiseJump(const QString& jumpDirection);

    void useTransliteration(bool value, const QString& langCode = "en");
    void useAutoSave(bool value) {m_autoSave = value;}

private slots:
    void contentChanged(int position, int charsRemoved, int charsAdded);
    void wordEditorChanged();

    void updateWordEditor();

    void changeSpeaker(const QString& newSpeaker, bool replaceAllOccurrences);
    void propagateTime(const QTime& time, int start, int end, bool negateTime);
    void selectTags(const QStringList& newTagList);
    void markWordAsCorrect(int blockNumber, int wordNumber);

    void insertSpeakerCompletion(const QString& completion);
    void insertTextCompletion(const QString& completion);
    void insertTransliterationCompletion(const QString &completion);

    void handleReply();
    void sendRequest(const QString& input, const QString& langCode);

private:
    static QTime getTime(const QString& text);
    static word makeWord(const QTime& t, const QString& s, const QStringList& tagList);
    QCompleter* makeCompleter(); 

    void loadTranscriptData(QFile& file);
    void setContent();
    void saveXml(QFile* file);
    void helpJumpToPlayer();
    void loadDictionary();

    block fromEditor(qint64 blockNumber) const;
    static QStringList listFromFile(const QString& fileName) ;

    bool settingContent{false}, updatingWordEditor{false}, dontUpdateWordEditor{false};
    bool m_transliterate{false}, m_autoSave{false};

    QVector<block> m_blocks;
    QString m_transcriptLang, m_punctuation{",.!;:"};
    QUrl m_transcriptUrl;
    Highlighter* m_highlighter = nullptr;
    qint64 highlightedBlock = -1, highlightedWord = -1;
    WordEditor* m_wordEditor = nullptr;
    ChangeSpeakerDialog* m_changeSpeaker = nullptr;
    TimePropagationDialog* m_propagateTime = nullptr;
    TagSelectionDialog* m_selectTag = nullptr;
    QCompleter *m_speakerCompleter = nullptr, *m_textCompleter = nullptr, *m_transliterationCompleter = nullptr;
    QStringList m_dictionary;
    std::set<QString> m_correctedWords;
    QString m_transliterateLangCode;
    QStringList m_lastReplyList;
    QNetworkAccessManager m_manager;
    QNetworkReply* m_reply = nullptr;
    QTimer* m_saveTimer = nullptr;
    int m_saveInterval{20};
};




class Highlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit Highlighter(QTextDocument *parent = nullptr) : QSyntaxHighlighter(parent) {};

    void clearHighlight()
    {
        blockToHighlight = -1;
        wordToHighlight = -1;
    }
    void setBlockToHighlight(qint64 blockNumber)
    {
        blockToHighlight = blockNumber;
        rehighlight();
    }
    void setWordToHighlight(int wordNumber)
    {
        wordToHighlight = wordNumber;
        rehighlight();
    }
    void setInvalidBlocks(const QList<int>& invalidBlocks)
    {
        invalidBlockNumbers = invalidBlocks;
        rehighlight();
    }
    void setInvalidWords(const QMultiMap<int, int>& invalidWordsMap)
    {
        invalidWords = invalidWordsMap;
        rehighlight();
    }
    void clearInvalidBlocks()
    {
        invalidBlockNumbers.clear();
    }

    void highlightBlock(const QString&) override;

private:
    int blockToHighlight{-1};
    int wordToHighlight{-1};
    QList<int> invalidBlockNumbers;
    QMultiMap<int, int> invalidWords;
};

