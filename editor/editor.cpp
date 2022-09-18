#include "editor.h"

#include <QPainter>
#include <QTextBlock>
#include <QFileDialog>
#include <QInputDialog>
#include <QStandardPaths>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QStringListModel>
#include <QMessageBox>
#include <QMenu>
#include <algorithm>
#include <QEventLoop>
#include <QDebug>

Editor::Editor(QWidget *parent)
    : TextEditor(parent),
    m_speakerCompleter(makeCompleter()), m_textCompleter(makeCompleter()), m_transliterationCompleter(makeCompleter()),
    m_dictionary(listFromFile(":/wordlists/english.txt")), m_transcriptLang("english"),
    timeStampExp(QRegularExpression(R"(\[(\d?\d:)?[0-5]?\d:[0-5]?\d(\.\d\d?\d?)?])")),
    speakerExp(QRegularExpression(R"(\[.*]:)")),
    m_saveTimer(new QTimer(this))
{
    connect(this->document(), &QTextDocument::contentsChange, this, &Editor::contentChanged);
    connect(this, &Editor::cursorPositionChanged, this, &Editor::updateWordEditor);
    connect(this, &Editor::cursorPositionChanged, this,
    [&]()
    {
        if (!m_blocks.isEmpty() && textCursor().blockNumber() < m_blocks.size())
            emit refreshTagList(m_blocks[textCursor().blockNumber()].tagList);
    });

    m_textCompleter->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    m_transliterationCompleter->setModel(new QStringListModel);

    loadDictionary();

    connect(m_speakerCompleter, QOverload<const QString &>::of(&QCompleter::activated),
            this, &Editor::insertSpeakerCompletion);
    connect(m_textCompleter, QOverload<const QString &>::of(&QCompleter::activated),
            this, &Editor::insertTextCompletion);
    connect(m_transliterationCompleter, QOverload<const QString &>::of(&QCompleter::activated),
            this, &Editor::insertTransliterationCompletion);

    
    connect(m_saveTimer, &QTimer::timeout, this, [this](){
        if (m_autoSave && m_transcriptUrl.isValid())
            transcriptSave();
    });
    m_saveTimer->start(m_saveInterval * 1000);

    m_blocks.append(fromEditor(0));
}

void Editor::setEditorFont(const QFont& font)
{
    document()->setDefaultFont(font);
    m_textCompleter->popup()->setFont(font);
    m_speakerCompleter->popup()->setFont(font);
    m_transliterationCompleter->popup()->setFont(font);
    setLineNumberAreaFont(font);
}



void Highlighter::highlightBlock(const QString& text)
{
    if (invalidBlockNumbers.contains(currentBlock().blockNumber())) {
        QTextCharFormat format;
        format.setForeground(Qt::red);
        setFormat(0, text.size(), format);
        return;
    }
    if (invalidWords.contains(currentBlock().blockNumber())) {
        auto invalidWordNumbers = invalidWords.values(currentBlock().blockNumber());
        auto speakerEnd = 0;
        auto speakerMatch = QRegularExpression(R"(\[.*]:)").match(text);
        if (speakerMatch.hasMatch())
            speakerEnd = speakerMatch.capturedEnd();

        auto words = text.mid(speakerEnd + 1).split(" ");
        int start = speakerEnd;

        QTextCharFormat format;
        format.setFontUnderline(true);
        format.setUnderlineColor(Qt::red);
        format.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);

        for (int i = 0; i < words.size() - 1; i++) {
            if (!invalidWordNumbers.contains(i))
                continue;
            for (int j = 0; j < i; j++) start += (words[j].size() + 1);
            int count = words[i].size();
            setFormat(start + 1, count, format);
            start = speakerEnd;
        }
    }
    if (blockToHighlight == -1)
        return;
    else if (currentBlock().blockNumber() == blockToHighlight) {
        int speakerEnd = 0;
        auto speakerMatch = QRegularExpression(R"(\[.*]:)").match(text);
        if (speakerMatch.hasMatch())
            speakerEnd = speakerMatch.capturedEnd();

        int timeStampStart = QRegularExpression(R"(\[(\d?\d:)?[0-5]?\d:[0-5]?\d(\.\d\d?\d?)?])").match(text).capturedStart();

        QTextCharFormat format;

        format.setForeground(QColor(Qt::blue).lighter(120));
        setFormat(0, speakerEnd, format);

        format.setForeground(Qt::green);
        setFormat(speakerEnd, timeStampStart, format);

        format.setForeground(Qt::red);
        setFormat(timeStampStart, text.size(), format);

        auto words = text.mid(speakerEnd + 1).split(" ");

        if (wordToHighlight != -1 && wordToHighlight < words.size()) {
            int start = speakerEnd;
            for (int i=0; i < wordToHighlight; i++) start += (words[i].size() + 1);
            int count = words[wordToHighlight].size();

            format.setFontUnderline(true);
            format.setUnderlineColor(Qt::green);
            format.setUnderlineStyle(QTextCharFormat::DashUnderline);
            format.setForeground(Qt::green);
            setFormat(start + 1, count, format);
        }
    }
}

void Editor::mousePressEvent(QMouseEvent *e)
{
    QPlainTextEdit::mousePressEvent(e);
    if (e->modifiers() == Qt::ControlModifier && !m_blocks.isEmpty())
        helpJumpToPlayer();
}

void Editor::keyPressEvent(QKeyEvent *event)
{
    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_R)
        createChangeSpeakerDialog();
    else if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_T)
        createTimePropagationDialog();

    auto checkPopupVisible = [](QCompleter* completer) {
        return completer && completer->popup()->isVisible();
    };

    if (checkPopupVisible(m_textCompleter)
            || checkPopupVisible(m_speakerCompleter)
            || checkPopupVisible(m_transliterationCompleter)
        ) {
        // The following keys are forwarded by the completer to the widget
       switch (event->key()) {
       case Qt::Key_Enter:
       case Qt::Key_Return:
       case Qt::Key_Escape:
       case Qt::Key_Tab:
       case Qt::Key_Backtab:
            event->ignore();
            return; // let the completer do default behavior
       default:
           break;
       }
    }

    TextEditor::keyPressEvent(event);

    QString blockText = textCursor().block().text();
    QString textTillCursor = blockText.left(textCursor().positionInBlock());
    QString completionPrefix;

    const bool shortcutPressed = (event->key() == Qt::Key_N && event->modifiers() == Qt::ControlModifier);
    const bool hasModifier = (event->modifiers() != Qt::NoModifier);
    const bool containsSpeakerBraces = blockText.leftRef(blockText.indexOf(" ")).contains("]:");

    static QString eow("~!@#$%^&*()_+{}|:\"<>?,./;'[]\\-="); // end of word

    if (!shortcutPressed && (hasModifier || event->text().isEmpty() || eow.contains(event->text().right(1)))) {
        m_speakerCompleter->popup()->hide();
        m_textCompleter->popup()->hide();
        m_transliterationCompleter->popup()->hide();
        return;
    }

    QCompleter *m_completer = nullptr;

    if (!textTillCursor.count(" ") && !textTillCursor.contains("]:") && !textTillCursor.contains("]")
            && textTillCursor.size() && containsSpeakerBraces) {
        // Complete speaker
        m_completer = m_speakerCompleter;

        completionPrefix = blockText.left(blockText.indexOf(" "));
        completionPrefix = completionPrefix.mid(1, completionPrefix.size() - 3);

        QList<QString> speakers;
        for (auto& a_block: qAsConst(m_blocks))
            if (!speakers.contains(a_block.speaker) && a_block.speaker != "")
                speakers.append(a_block.speaker);

        m_speakerCompleter->setModel(new QStringListModel(speakers, m_speakerCompleter));
    }
    else {
        if (m_blocks[textCursor().blockNumber()].timeStamp.isValid()
                && textTillCursor.count(" ") == blockText.count(" "))
            return;

        int index = textTillCursor.count(" ");
        completionPrefix = blockText.split(" ")[index];

        if (completionPrefix.isEmpty()){
            m_textCompleter->popup()->hide();
            m_transliterationCompleter->popup()->hide();
            return;
        }

        if (completionPrefix.size() < 2 && !m_transliterate) {
            m_textCompleter->popup()->hide();
            return;
        }

        // Complete text
        if (!m_transliterate)
            m_completer = m_textCompleter;
        else
            m_completer = m_transliterationCompleter;
    }

    if (!m_completer)
        return;

    if (m_completer == m_transliterationCompleter) {
        QTimer replyTimer;
        replyTimer.setSingleShot(true);
        QEventLoop loop;
        connect(this, &Editor::replyCame, &loop, &QEventLoop::quit);
        connect(&replyTimer, &QTimer::timeout, &loop,
                [&]() {
                    emit message("Reply Timeout, Network Connection is slow or inaccessible", 2000);
                    loop.quit();
        });

        sendRequest(completionPrefix, m_transliterateLangCode);
        replyTimer.start(1000);
        loop.exec();

        dynamic_cast<QStringListModel*>(m_completer->model())->setStringList(m_lastReplyList);
    }


    if (m_completer != m_transliterationCompleter && completionPrefix != m_completer->completionPrefix()) {
        m_completer->setCompletionPrefix(completionPrefix);
    }
    m_completer->popup()->setCurrentIndex(m_completer->completionModel()->index(0, 0));

    QRect cr = cursorRect();
    cr.setWidth(m_completer->popup()->sizeHintForColumn(0)
                + m_completer->popup()->verticalScrollBar()->sizeHint().width());
    m_completer->complete(cr);
}

void Editor::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createStandardContextMenu();

    QString blockText = textCursor().block().text();
    QString textTillCursor = blockText.left(textCursor().positionInBlock());

    const bool containsSpeakerBraces = blockText.leftRef(blockText.indexOf(" ")).contains("]:");
    const bool containsTimeStamp = blockText.trimmed() != "" && blockText.trimmed().back() == ']';
    bool isAWordUnderCursor = false;
    int wordNumber;

    if ((containsSpeakerBraces && textTillCursor.count(" ") > 0) || !containsSpeakerBraces) {
        if (m_blocks.size() > textCursor().blockNumber() &&
                !(containsTimeStamp && textTillCursor.count(" ") == blockText.count(" "))) {
            isAWordUnderCursor = true;

            if (containsSpeakerBraces) {
                auto textWithoutSpeaker = textTillCursor.split("]:").last();
                wordNumber = textWithoutSpeaker.trimmed().count(" ");
            }
            else
                wordNumber = textTillCursor.trimmed().count(" ");
        }
    }

    if (isAWordUnderCursor) {
        auto markAsCorrectAction = new QAction;
        markAsCorrectAction->setText("Mark As Correct");

        connect(markAsCorrectAction, &QAction::triggered, this,
                [this, wordNumber]()
                {
                    markWordAsCorrect(textCursor().blockNumber(), wordNumber);
        });

        menu->addAction(markAsCorrectAction);
    }

    menu->exec(event->globalPos());
    delete menu;
}

void Editor::transcriptOpen()
{
    QFileDialog fileDialog(this);
    fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    fileDialog.setWindowTitle(tr("Open File"));
    fileDialog.setDirectory(QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).value(0, QDir::homePath()));

    if (fileDialog.exec() == QDialog::Accepted) {
        QUrl *fileUrl = new QUrl(fileDialog.selectedUrls().constFirst());
        m_transcriptUrl = *fileUrl;
        QFile transcriptFile(fileUrl->toLocalFile());

        if (!transcriptFile.open(QIODevice::ReadOnly)) {
            emit message(transcriptFile.errorString());
            return;
        }

        m_saveTimer->stop();

        loadTranscriptData(transcriptFile);

        if (m_transcriptLang == "")
            m_transcriptLang = "english";

        loadDictionary(); 

        setContent();

        if (m_transcriptLang != "")
            emit message("Opened transcript " + fileUrl->fileName() + " Language: " + m_transcriptLang);
        else
            emit message("Opened transcript " + fileUrl->fileName());

        m_saveTimer->start(m_saveInterval * 1000);
    }
}

void Editor::transcriptSave()
{
    if (m_transcriptUrl.isEmpty())
        transcriptSaveAs();
    else {
        auto *file = new QFile(m_transcriptUrl.toLocalFile());
        if (!file->open(QIODevice::WriteOnly | QFile::Truncate)) {
            emit message(file->errorString());
            return;
        }
        saveXml(file);
        emit message("File Saved " + m_transcriptUrl.toLocalFile());
    }
}

void Editor::transcriptSaveAs()
{
    QFileDialog fileDialog(this);
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    fileDialog.setWindowTitle(tr("Save Transcript"));
    fileDialog.setDirectory(QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).value(0, QDir::homePath()));

    if (fileDialog.exec() == QDialog::Accepted) {
        auto fileUrl = QUrl(fileDialog.selectedUrls().constFirst());

        if (!document()->isEmpty()) {
            auto *file = new QFile(fileUrl.toLocalFile());
            if (!file->open(QIODevice::WriteOnly)) {
                emit message(file->errorString());
                return;
            }
            saveXml(file);
            emit message("File Saved " + fileUrl.toLocalFile());
        }
    }
}

void Editor::transcriptClose()
{
    if (m_transcriptUrl.isEmpty()) {
        emit message("No file open");
        return;
    }


    emit message("Closing file " + m_transcriptUrl.toLocalFile());
    m_transcriptUrl.clear();
    m_blocks.clear();
    m_transcriptLang = "english";
    
    loadDictionary();
    clear();
}

void Editor::showBlocksFromData()
{
    for (auto& m_block: qAsConst(m_blocks)) {
        qDebug() << m_block.timeStamp << m_block.speaker << m_block.text << m_block.tagList;
        for (auto& m_word: qAsConst(m_block.words)) {
            qDebug() << "   " << m_word.timeStamp << m_word.text << m_word.tagList;
        }
    }
}

void Editor::highlightTranscript(const QTime& elapsedTime)
{
    int blockToHighlight = -1;
    int wordToHighlight = -1;

    if (!m_blocks.isEmpty()) {
        for (int i=0; i < m_blocks.size(); i++) {
            if (m_blocks[i].timeStamp > elapsedTime) {
                blockToHighlight = i;
                break;
            }
        }
    }

    if (blockToHighlight != highlightedBlock) {
        highlightedBlock = blockToHighlight;
        if (!m_highlighter)
            m_highlighter = new Highlighter(document());
        m_highlighter->setBlockToHighlight(blockToHighlight);
    }

    if (blockToHighlight == -1)
        return;

    for (int i = 0; i < m_blocks[blockToHighlight].words.size(); i++) {
        if (m_blocks[blockToHighlight].words[i].timeStamp > elapsedTime) {
            wordToHighlight = i;
            break;
        }
    }

    if (wordToHighlight != highlightedWord) {
        highlightedWord = wordToHighlight;
        m_highlighter->setWordToHighlight(wordToHighlight);
    }
}

QTime Editor::getTime(const QString& text)
{
    if (text.contains(".")) {
        if (text.count(":") == 2) return QTime::fromString(text, "h:m:s.z");
        return QTime::fromString(text, "m:s.z");
    }
    else {
        if (text.count(":") == 2) return QTime::fromString(text, "h:m:s");
        return QTime::fromString(text, "m:s");
    }
}

word Editor::makeWord(const QTime& t, const QString& s, const QStringList& tagList)
{
    word w = {t, s, tagList};
    return w;
}

QCompleter* Editor::makeCompleter()
{   
    auto completer = new QCompleter(this); 
    completer->setWidget(this); 
    completer->setCaseSensitivity(Qt::CaseInsensitive); 
    completer->setWrapAround(false); 
    completer->setCompletionMode(QCompleter::PopupCompletion); 

    return completer;
}

block Editor::fromEditor(qint64 blockNumber) const
{
    QTime timeStamp;
    QVector<word> words;
    QString text, speaker, blockText(document()->findBlockByNumber(blockNumber).text());

    QRegularExpressionMatch match = timeStampExp.match(blockText);
    if (match.hasMatch()) {
        QString matchedTimeStampString = match.captured();
        if (blockText.mid(match.capturedEnd()).trimmed() == "") {
            // Get timestamp for string after removing the enclosing []
            timeStamp = getTime(matchedTimeStampString.mid(1,matchedTimeStampString.size() - 2));
            text = blockText.split(matchedTimeStampString)[0];
        }
    }

    match = speakerExp.match(blockText);
    if (match.hasMatch()) {
        speaker = match.captured();
        if (text != "")
            text = text.split(speaker)[1];
        speaker = speaker.left(speaker.size() - 2);
        speaker = speaker.right(speaker.size() - 1);
    }

    if (text == "")
        text = blockText.trimmed();
    else
        text = text.trimmed();

    auto list = text.split(" ");
    for (auto& m_word: qAsConst(list)) {
        words.append(makeWord(QTime(), m_word, QStringList()));
    }

    block b = {timeStamp, text, speaker, QStringList(), words};
    return b;
}

void Editor::loadTranscriptData(QFile& file)
{
    QXmlStreamReader reader(&file);
    m_transcriptLang = "";
    m_blocks.clear();
    if (reader.readNextStartElement()) {
        if (reader.name() == "transcript") {
            m_transcriptLang = reader.attributes().value("lang").toString();

            while(reader.readNextStartElement()) {
                if(reader.name() == "line") {
                    auto blockTimeStamp = getTime(reader.attributes().value("timestamp").toString());
                    auto blockText = QString("");
                    auto blockSpeaker = reader.attributes().value("speaker").toString();
                    auto tagString = reader.attributes().value("tags").toString();
                    QStringList tagList;
                    if (tagString != "")
                        tagList = tagString.split(",");

                    struct block line = {blockTimeStamp, "", blockSpeaker, tagList, QVector<word>()};
                    while(reader.readNextStartElement()){
                        if(reader.name() == "word"){
                            auto wordTimeStamp  = getTime(reader.attributes().value("timestamp").toString());
                            auto wordTagString  = reader.attributes().value("tags").toString();
                            auto wordText       = reader.readElementText();
                            QStringList wordTagList;
                            if (wordTagString != "")
                                wordTagList = wordTagString.split(",");

                            blockText += (wordText + " ");
                            line.words.append(makeWord(wordTimeStamp, wordText, wordTagList));
                        }
                        else
                            reader.skipCurrentElement();
                    }
                    line.text = blockText.trimmed();
                    m_blocks.append(line);
                }
                else
                    reader.skipCurrentElement();
            }
        }
        else
            reader.raiseError(QObject::tr("Incorrect file"));
    }
}

void Editor::saveXml(QFile* file)
{
    QXmlStreamWriter writer(file);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writer.writeStartElement("transcript");

    if (m_transcriptLang != "")
        writer.writeAttribute("lang", m_transcriptLang);

    for (auto& a_block: qAsConst(m_blocks)) {
        if (a_block.text != "") {
            auto timeStamp = a_block.timeStamp;
            QString timeStampString = timeStamp.toString("hh:mm:ss.zzz");
            auto speaker = a_block.speaker;

            writer.writeStartElement("line");
            writer.writeAttribute("timestamp", timeStampString);
            writer.writeAttribute("speaker", speaker);

            if (!a_block.tagList.isEmpty())
                writer.writeAttribute("tags", a_block.tagList.join(","));

            for (auto& a_word: qAsConst(a_block.words)) {
                writer.writeStartElement("word");
                writer.writeAttribute("timestamp", a_word.timeStamp.toString("hh:mm:ss.zzz"));

                if (!a_word.tagList.isEmpty())
                    writer.writeAttribute("tags", a_word.tagList.join(","));

                writer.writeCharacters(a_word.text);
                writer.writeEndElement();
            }
            writer.writeEndElement();
        }
    }
    writer.writeEndElement();
    file->close();
    delete file;
}

void Editor::helpJumpToPlayer()
{
    auto currentBlockNumber = textCursor().blockNumber();
    auto timeToJump = QTime(0, 0);

    if (m_blocks[currentBlockNumber].timeStamp.isNull())
        return;

    int positionInBlock = textCursor().positionInBlock();
    auto blockText = textCursor().block().text();
    auto textBeforeCursor = blockText.left(positionInBlock);
    int wordNumber = textBeforeCursor.count(" ");
    if (m_blocks[currentBlockNumber].speaker != "" || textCursor().block().text().contains("[]:"))
        wordNumber--;

    for (int i = currentBlockNumber - 1; i >= 0; i--) {
        if (m_blocks[i].timeStamp.isValid()) {
            timeToJump = m_blocks[i].timeStamp;
            break;
        }
    }

    // If we can jump to a word, then do so
    if (wordNumber >= 0 &&
        wordNumber < m_blocks[currentBlockNumber].words.size() &&
        m_blocks[currentBlockNumber].words[wordNumber].timeStamp.isValid()
        ) {
        for (int i = wordNumber - 1; i >= 0; i--) {
            if (m_blocks[currentBlockNumber].words[i].timeStamp.isValid()) {
                timeToJump = m_blocks[currentBlockNumber].words[i].timeStamp;
                emit jumpToPlayer(timeToJump);
                return;
            }
        }
    }

    emit jumpToPlayer(timeToJump);
}

void Editor::loadDictionary()
{
    m_correctedWords.clear();
    m_dictionary.clear();

    auto dictionaryFileName = QString(":/wordlists/%1.txt").arg(m_transcriptLang);
    m_dictionary = listFromFile(dictionaryFileName);

    auto correctedWordsList = listFromFile(QString("corrected_words_%1.txt").arg(m_transcriptLang));
    if (!correctedWordsList.isEmpty()) {
        std::copy(correctedWordsList.begin(),
                  correctedWordsList.end(),
                  std::inserter(m_correctedWords, m_correctedWords.begin()));

        for (auto& a_word: m_correctedWords) {
            m_dictionary.insert
            (
                std::upper_bound(m_dictionary.begin(), m_dictionary.end(), a_word),
                a_word
            );
        }
    }
    m_textCompleter->setModel(new QStringListModel(m_dictionary, m_textCompleter));

    if (!m_highlighter)
        return;

    QMultiMap<int, int> invalidWords;
    for (int i = 0; i < m_blocks.size(); i++) {
        for (int j = 0; j < m_blocks[i].words.size(); j++) {
            auto wordText = m_blocks[i].words[j].text.toLower();

            if (wordText != "" && m_punctuation.contains(wordText.back()))
                wordText = wordText.left(wordText.size() - 1);

            if (!std::binary_search(m_dictionary.begin(),
                                    m_dictionary.end(),
                                    wordText)
                )
                invalidWords.insert(i, j);
        }
    }
    m_highlighter->setInvalidWords(invalidWords);
    m_highlighter->rehighlight();
}

QStringList Editor::listFromFile(const QString& fileName)
{
    QStringList words;

    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return {};

    while (!file.atEnd()) {
        QByteArray line = file.readLine();
        if (!line.isEmpty())
            words << QString::fromUtf8(line.trimmed());
    }

    return words;
}

void Editor::setContent()
{
    if (!settingContent) {
        settingContent = true;

        if (m_highlighter)
            delete m_highlighter;

        QString content("");
        for (auto& a_block: qAsConst(m_blocks)) {
            auto blockText = "[" + a_block.speaker + "]: " + a_block.text + " [" + a_block.timeStamp.toString("hh:mm:ss.zzz") + "]";
            content.append(blockText + "\n");
        }
        setPlainText(content.trimmed());

        m_highlighter = new Highlighter(document());

        QList<int> invalidBlocks;
        QMultiMap<int, int> invalidWords;
        for (int i = 0; i < m_blocks.size(); i++) {
            if (m_blocks[i].timeStamp.isNull())
                invalidBlocks.append(i);
            else {
                for (int j = 0; j < m_blocks[i].words.size(); j++) {
                    auto wordText = m_blocks[i].words[j].text.toLower();

                    if (wordText != "" && m_punctuation.contains(wordText.back()))
                        wordText = wordText.left(wordText.size() - 1);

                    if (!std::binary_search(m_dictionary.begin(),
                                            m_dictionary.end(),
                                            wordText)
                       )
                        invalidWords.insert(i, j);
                }
            }
        }

        m_highlighter->setInvalidBlocks(invalidBlocks);
        m_highlighter->setInvalidWords(invalidWords);
        m_highlighter->setBlockToHighlight(highlightedBlock);
        m_highlighter->setWordToHighlight(highlightedWord);

        settingContent = false;
    }
}

void Editor::contentChanged(int position, int charsRemoved, int charsAdded)
{
    // If chars aren't added or deleted then return
    if (!(charsAdded || charsRemoved) || settingContent)
        return;
    else if (m_blocks.isEmpty()) { // If block data is empty (i.e. no file opened) just fill them from editor
        for (int i = 0; i < document()->blockCount(); i++)
            m_blocks.append(fromEditor(i));
        return;
    }

    delete m_highlighter;
    m_highlighter = new Highlighter(this->document());

    int currentBlockNumber = textCursor().blockNumber();

    if(m_blocks.size() != blockCount()) {
        auto blocksChanged = m_blocks.size() - blockCount();
        if (blocksChanged > 0) { // Blocks deleted
            qInfo() << "[Lines Deleted]" << QString("%1 lines deleted").arg(QString::number(blocksChanged));
            for (int i = 1; i <= blocksChanged; i++)
                m_blocks.removeAt(currentBlockNumber + 1);
        }
        else { // Blocks added
            qInfo() << "[Lines Inserted]" << QString("%1 lines inserted").arg(QString::number(-blocksChanged));
            for (int i = 1; i <= -blocksChanged; i++) {
                if (document()->findBlockByNumber(currentBlockNumber + blocksChanged).text().trimmed() == "")
                    m_blocks.insert(currentBlockNumber + blocksChanged, fromEditor(currentBlockNumber - i));
                else
                    m_blocks.insert(currentBlockNumber + blocksChanged + 1, fromEditor(currentBlockNumber - i + 1));
            }
        }
    }
    
    auto currentBlockFromEditor = fromEditor(currentBlockNumber);
    auto& currentBlockFromData = m_blocks[currentBlockNumber];

    if (currentBlockFromData.speaker != currentBlockFromEditor.speaker) {
        qInfo() << "[Speaker Changed]"
                << QString("line number: %1").arg(QString::number(currentBlockNumber + 1))
                << QString("initial: %1").arg(currentBlockFromData.speaker)
                << QString("final: %1").arg(currentBlockFromEditor.speaker);

        currentBlockFromData.speaker = currentBlockFromEditor.speaker;
    }

    if (currentBlockFromData.timeStamp != currentBlockFromEditor.timeStamp) {
        currentBlockFromData.timeStamp = currentBlockFromEditor.timeStamp;
        qInfo() << "[TimeStamp Changed]"
                << QString("line number: %1, %2").arg(QString::number(currentBlockNumber + 1), currentBlockFromEditor.timeStamp.toString("hh:mm:ss.zzz"));
    }

    if (currentBlockFromData.text != currentBlockFromEditor.text) {
        qInfo() << "[Text Changed]"
                << QString("line number: %1").arg(QString::number(currentBlockNumber + 1))
                << QString("initial: %1").arg(currentBlockFromData.text)
                << QString("final: %1").arg(currentBlockFromEditor.text);

        currentBlockFromData.text = currentBlockFromEditor.text;
        auto tagList = currentBlockFromData.tagList;

        auto& wordsFromEditor = currentBlockFromEditor.words;
        auto& wordsFromData = currentBlockFromData.words;

        int wordsDifference = wordsFromEditor.size() - wordsFromData.size();
        int diffStart{-1}, diffEnd{-1};

        for (int i = 0; i < wordsFromEditor.size() && i < wordsFromData.size(); i++)
            if (wordsFromEditor[i].text != wordsFromData[i].text) {
                diffStart = i;
                break;
            }

        if (diffStart == -1)
            diffStart = wordsFromEditor.size() - 1;
        for (int i = 0; i <= diffStart; i++)
            if (i < wordsFromData.size())
                wordsFromEditor[i].timeStamp = wordsFromData[i].timeStamp;
        if (!wordsDifference) {
            for (int i = diffStart; i < wordsFromEditor.size(); i++)
                wordsFromEditor[i].timeStamp = wordsFromData[i].timeStamp;
        }

        if (wordsDifference > 0) {
            for (int i = wordsFromEditor.size() - 1, j = wordsFromData.size() - 1; j > diffStart; i--, j--)
                if (wordsFromEditor[i].text == wordsFromData[j].text)
                    wordsFromEditor[i].timeStamp = wordsFromData[j].timeStamp;
        }
        else if (wordsDifference < 0) {
            for (int i = wordsFromEditor.size() - 1, j = wordsFromData.size() - 1; i > diffStart; i--, j--)
                if (wordsFromEditor[i].text == wordsFromData[j].text)
                    wordsFromEditor[i].timeStamp = wordsFromData[j].timeStamp;
        }

        currentBlockFromData = currentBlockFromEditor;
        currentBlockFromData.tagList = tagList;
    }

    m_highlighter->setBlockToHighlight(highlightedBlock);
    m_highlighter->setWordToHighlight(highlightedWord);

    QList<int> invalidBlocks;
    QMultiMap<int, int> invalidWords;
    for (int i = 0; i < m_blocks.size(); i++) {
        if (m_blocks[i].timeStamp.isNull())
            invalidBlocks.append(i);
        else {
            for (int j = 0; j < m_blocks[i].words.size(); j++) {
                auto wordText = m_blocks[i].words[j].text.toLower();

                if (wordText != "" && m_punctuation.contains(wordText.back()))
                    wordText = wordText.left(wordText.size() - 1);

                if (!std::binary_search(m_dictionary.begin(),
                                        m_dictionary.end(),
                                        wordText)
                    )
                    invalidWords.insert(i, j);
            }
        }
    }
    m_highlighter->setInvalidBlocks(invalidBlocks);
    m_highlighter->setInvalidWords(invalidWords);

    updateWordEditor();
}

void Editor::jumpToHighlightedLine()
{
    if (highlightedBlock == -1)
        return;
    QTextCursor cursor(document()->findBlockByNumber(highlightedBlock));
    setTextCursor(cursor);
}

void Editor::splitLine(const QTime& elapsedTime)
{
    auto cursor = textCursor();
    if (cursor.blockNumber() != highlightedBlock)
        return;

    int positionInBlock = cursor.positionInBlock();
    auto blockText = cursor.block().text();
    auto textBeforeCursor = blockText.left(positionInBlock);
    auto textAfterCursor = blockText.right(blockText.size() - positionInBlock);
    auto cutWordLeft = textBeforeCursor.split(" ").last();
    auto cutWordRight = textAfterCursor.split(" ").first();
    int wordNumber = textBeforeCursor.count(" ");

    if (m_blocks[highlightedBlock].speaker != "" || blockText.contains("[]:"))
        wordNumber--;
    if (wordNumber < 0 || wordNumber >= m_blocks[highlightedBlock].words.size())
        return;

    if (textBeforeCursor.contains("]:"))
        textBeforeCursor = textBeforeCursor.split("]:").last();
    if (textAfterCursor.contains("["))
        textAfterCursor = textAfterCursor.split("[").first();

    auto timeStampOfCutWord = m_blocks[highlightedBlock].words[wordNumber].timeStamp;
    auto tagsOfCutWord = m_blocks[highlightedBlock].words[wordNumber].tagList;
    QVector<word> words;
    int sizeOfWordsAfter = m_blocks[highlightedBlock].words.size() - wordNumber - 1;

    if (cutWordRight != "")
        words.append(makeWord(timeStampOfCutWord, cutWordRight, tagsOfCutWord));
    for (int i = 0; i < sizeOfWordsAfter; i++) {
        words.append(m_blocks[highlightedBlock].words[wordNumber + 1]);
        m_blocks[highlightedBlock].words.removeAt(wordNumber + 1);
    }

    if (cutWordLeft == "")
        m_blocks[highlightedBlock].words.removeAt(wordNumber);
    else {
        m_blocks[highlightedBlock].words[wordNumber].text = cutWordLeft;
        m_blocks[highlightedBlock].words[wordNumber].timeStamp = elapsedTime;
    }

    block blockToInsert = {m_blocks[highlightedBlock].timeStamp,
                           textAfterCursor.trimmed(),
                           m_blocks[highlightedBlock].speaker,
                           m_blocks[highlightedBlock].tagList,
                           words};
    m_blocks.insert(highlightedBlock + 1, blockToInsert);

    m_blocks[highlightedBlock].text = textBeforeCursor.trimmed();
    m_blocks[highlightedBlock].timeStamp = elapsedTime;

    setContent();
    updateWordEditor();

    qInfo() << "[Line Split]"
            << QString("line number: %1").arg(QString::number(highlightedBlock + 1))
            << QString("word number: %1, %2").arg(QString::number(wordNumber + 1), cutWordLeft + cutWordRight);
}

void Editor::mergeUp()
{
    auto blockNumber = textCursor().blockNumber();
    auto previousBlockNumber = blockNumber - 1;

    if (m_blocks.isEmpty() || blockNumber == 0 || m_blocks[blockNumber].speaker != m_blocks[previousBlockNumber].speaker)
        return;

    auto currentWords = m_blocks[blockNumber].words;

    m_blocks[previousBlockNumber].words.append(currentWords);                 // Add current words to previous block
    m_blocks[previousBlockNumber].timeStamp = m_blocks[blockNumber].timeStamp;  // Update time stamp of previous block
    m_blocks[previousBlockNumber].text.append(" " + m_blocks[blockNumber].text);// Append text to previous block

    m_blocks.removeAt(blockNumber);
    setContent();
    updateWordEditor();

    QTextCursor cursor(document()->findBlockByNumber(previousBlockNumber));
    setTextCursor(cursor);
    centerCursor();

    qInfo() << "[Merge Up]"
            << QString("line number: %1, %2").arg(QString::number(previousBlockNumber + 1), QString::number(blockNumber + 1))
            << QString("final line: %1, %2").arg(QString::number(previousBlockNumber + 1), m_blocks[previousBlockNumber].text);
}

void Editor::mergeDown()
{
    auto blockNumber = textCursor().blockNumber();
    auto nextBlockNumber = blockNumber + 1;

    if (m_blocks.isEmpty() || blockNumber == m_blocks.size() - 1 || m_blocks[blockNumber].speaker != m_blocks[nextBlockNumber].speaker)
        return;

    auto currentWords = m_blocks[blockNumber].words;

    auto temp = m_blocks[nextBlockNumber].words;
    m_blocks[nextBlockNumber].words = currentWords;
    m_blocks[nextBlockNumber].words.append(temp);

    auto tempText = m_blocks[nextBlockNumber].text;
    m_blocks[nextBlockNumber].text = m_blocks[blockNumber].text;
    m_blocks[nextBlockNumber].text.append(" " + tempText);

    m_blocks.removeAt(blockNumber);
    setContent();
    updateWordEditor();

    QTextCursor cursor(document()->findBlockByNumber(blockNumber));
    setTextCursor(cursor);
    centerCursor();

    qInfo() << "[Merge Down]"
            << QString("line number: %1, %2").arg(QString::number(blockNumber + 1), QString::number(nextBlockNumber + 1))
            << QString("final line: %1, %2").arg(QString::number(blockNumber + 1), m_blocks[nextBlockNumber].text);
}

void Editor::createChangeSpeakerDialog()
{
    if (m_blocks.isEmpty())
        return;

    m_changeSpeaker = new ChangeSpeakerDialog(this);
    m_changeSpeaker->setModal(true);
    m_changeSpeaker->setAttribute(Qt::WA_DeleteOnClose);

    QSet<QString> speakers;
    for (auto& a_block: qAsConst(m_blocks))
        speakers.insert(a_block.speaker);

    m_changeSpeaker->addItems(speakers.values());
    m_changeSpeaker->setCurrentSpeaker(m_blocks.at(textCursor().blockNumber()).speaker);

    connect(m_changeSpeaker,
            &ChangeSpeakerDialog::accepted,
            this,
            [&]() {
                changeSpeaker(m_changeSpeaker->speaker(), m_changeSpeaker->replaceAll());
            }
    );
    m_changeSpeaker->show();
}

void Editor::createTimePropagationDialog()
{
    if (m_blocks.isEmpty())
        return;

    m_propagateTime = new TimePropagationDialog(this);
    m_propagateTime->setModal(true);
    m_propagateTime->setAttribute(Qt::WA_DeleteOnClose);

    m_propagateTime->setBlockRange(textCursor().blockNumber() + 1, blockCount());

    connect(m_propagateTime,
            &TimePropagationDialog::accepted,
            this,
            [&]() {
                propagateTime(m_propagateTime->time(),
                              m_propagateTime->blockStart(),
                              m_propagateTime->blockEnd(),
                              m_propagateTime->negateTime());
            }
    );
    m_propagateTime->show();
}

void Editor::createTagSelectionDialog()
{
    if (m_blocks.isEmpty())
        return;

    m_selectTag = new TagSelectionDialog(this);
    m_selectTag->setModal(true);
    m_selectTag->setAttribute(Qt::WA_DeleteOnClose);

    m_selectTag->markExistingTags(m_blocks[textCursor().blockNumber()].tagList);

    connect(m_selectTag,
            &TagSelectionDialog::accepted,
            this,
            [&]() {
                selectTags(m_selectTag->tagList());
            }
    );
    m_selectTag->show();
}

void Editor::insertTimeStamp(const QTime& elapsedTime)
{
    auto blockNumber = textCursor().blockNumber();

    if (m_blocks.size() <= blockNumber)
        return;

    m_blocks[blockNumber].timeStamp = elapsedTime;

    dontUpdateWordEditor = true;
    setContent();
    QTextCursor cursor(document()->findBlockByNumber(blockNumber));
    cursor.movePosition(QTextCursor::EndOfBlock);
    setTextCursor(cursor);
    centerCursor();
    dontUpdateWordEditor = false;

    qInfo() << "[Inserted TimeStamp from Player]"
            << QString("line number: %1, timestamp: %2").arg(QString::number(blockNumber), elapsedTime.toString("hh:mm:ss.zzz"));

}

void Editor::changeTranscriptLang()
{
    auto newLang = QInputDialog::getText(this, "Change Transcript Language", "Current Language: " + m_transcriptLang);
    m_transcriptLang = newLang.toLower();

    loadDictionary();
}

void Editor::speakerWiseJump(const QString& jumpDirection)
{
    auto& blockNumber = highlightedBlock;

    if (blockNumber == -1) {
        emit message("Highlighted block not present");
        return;
    }

    auto speakerName = m_blocks[blockNumber].speaker;
    int blockToJump{-1};

    if (jumpDirection == "up") {
        for (int i = blockNumber - 1; i >= 0; i--)
            if (speakerName == m_blocks[i].speaker) {
                blockToJump = i;
                break;
            }
    }
    else if (jumpDirection == "down") {
        for (int i = blockNumber + 1; i < m_blocks.size(); i++)
            if (speakerName == m_blocks[i].speaker) {
                blockToJump = i;
                break;
            }
    }

    if (blockToJump == -1) {
        emit message("Couldn't find a block to jump");
        return;
    }

    QTime timeToJump(0, 0);

    for (int i = blockToJump - 1; i >= 0; i--) {
        if (m_blocks[i].timeStamp.isValid()) {
            timeToJump = m_blocks[i].timeStamp;
            break;
        }
    }

    emit jumpToPlayer(timeToJump);
}

void Editor::wordWiseJump(const QString& jumpDirection)
{
    auto& wordNumber = highlightedWord;

    if (highlightedBlock == -1 || wordNumber == -1) {
        emit message("Highlighted block or word not present");
        return;
    }

    auto& highlightedBlockWords = m_blocks[highlightedBlock].words;
    QTime timeToJump;
    int wordToJump{-1};

    if (jumpDirection == "left")
        wordToJump = wordNumber - 1;
    else if (jumpDirection == "right")
        wordToJump = wordNumber + 1;

    if (wordToJump < 0 || wordToJump >= highlightedBlockWords.size()) {
        emit message("Can't jump, end of block reached!", 2000);
        return;
    }

    if (jumpDirection == "left") {
        if (wordToJump == 0){
            timeToJump = QTime(0, 0);
            for (int i = highlightedBlock - 1; i >= 0; i--) {
                if (m_blocks[i].timeStamp.isValid()) {
                    timeToJump = m_blocks[i].timeStamp;
                    break;
                }
            }
        }
        else {
            for (int i = wordToJump - 1; i >= 0; i--)
                if (highlightedBlockWords[i].timeStamp.isValid()) {
                    timeToJump = highlightedBlockWords[i].timeStamp;
                    break;
                }
        }
    }
    
    if (jumpDirection == "right")
        timeToJump = highlightedBlockWords[wordToJump - 1].timeStamp;

    if (timeToJump.isNull()) {
        emit message("Couldn't find a word to jump to");
        return;
    }

    emit jumpToPlayer(timeToJump);
}


void Editor::blockWiseJump(const QString& jumpDirection)
{
    if (highlightedBlock == -1)
        return;

    int blockToJump{-1};

    if (jumpDirection == "up")
        blockToJump = highlightedBlock - 1;
    else if (jumpDirection == "down")
        blockToJump = highlightedBlock + 1;

    if (blockToJump == -1 || blockToJump == blockCount())
        return;

    QTime timeToJump;

    if (jumpDirection == "up") {
        timeToJump = QTime(0, 0);
        for (int i = blockToJump - 1; i >= 0; i--) {
            if (m_blocks[i].timeStamp.isValid()) {
                timeToJump = m_blocks[i].timeStamp;
                break;
            }
        }
    }
    else if (jumpDirection == "down")
        timeToJump = m_blocks[highlightedBlock].timeStamp;

    emit jumpToPlayer(timeToJump);

}

void Editor::useTransliteration(bool value, const QString& langCode)
{
    m_transliterate = value;
    m_transliterateLangCode = langCode;
}

void Editor::updateWordEditor()
{
    if (!m_wordEditor || dontUpdateWordEditor)
        return;

    updatingWordEditor = true;

    auto blockNumber = textCursor().blockNumber();

    if (blockNumber >= m_blocks.size()) {
        m_wordEditor->clear();
        return;
    }

    m_wordEditor->refreshWords(m_blocks[blockNumber].words);

    updatingWordEditor = false;
}

void Editor::wordEditorChanged()
{
    auto editorBlockNumber = textCursor().blockNumber();

    if (document()->isEmpty() || m_blocks.isEmpty())
        m_blocks.append(fromEditor(0));

    if (settingContent || updatingWordEditor || editorBlockNumber >= m_blocks.size())
        return;

    auto& block = m_blocks[editorBlockNumber];
    if (block.words.isEmpty()) {
        block.words = m_wordEditor->currentWords();
        return;
    }

    auto& words = block.words;
    words.clear();

    QString blockText;
    words = m_wordEditor->currentWords();
    for (auto& a_word: words)
        blockText += a_word.text + " ";
    block.text = blockText.trimmed();

    dontUpdateWordEditor = true;
    setContent();
    QTextCursor cursor(document()->findBlockByNumber(editorBlockNumber));
    setTextCursor(cursor);
    centerCursor();
    dontUpdateWordEditor = false;
}

void Editor::changeSpeaker(const QString& newSpeaker, bool replaceAllOccurrences)
{
    if (m_blocks.isEmpty())
        return;
    auto blockNumber = textCursor().blockNumber();
    auto blockSpeaker = m_blocks[blockNumber].speaker;

    if (!replaceAllOccurrences)
        m_blocks[blockNumber].speaker = newSpeaker;
    else {
        for (auto& a_block: m_blocks){
            if (a_block.speaker == blockSpeaker)
                a_block.speaker = newSpeaker;
        }
    }

    setContent();
    QTextCursor cursor(document()->findBlockByNumber(blockNumber));
    setTextCursor(cursor);
    centerCursor();

    qInfo() << "[Speaker Changed]"
            << QString("line number: %1").arg(QString::number(blockNumber + 1))
            << QString("initial: %1").arg(blockSpeaker)
            << QString("final: %1").arg(newSpeaker);
}

void Editor::propagateTime(const QTime& time, int start, int end, bool negateTime)
{
    if (time.isNull()) {
        QMessageBox errorBox(QMessageBox::Critical, "Error", "Invalid Time Selected", QMessageBox::Ok);
        errorBox.exec();
        return;
    }
    else if (start < 1 || end > blockCount() || start > end) {
        QMessageBox errorBox(QMessageBox::Critical, "Error", "Invalid Block Range Selected", QMessageBox::Ok);
        errorBox.exec();
        return;
    }

    for (int i = start - 1; i < end; i++) {
        auto& currentTimeStamp = m_blocks[i].timeStamp;

        if (currentTimeStamp.isNull())
            currentTimeStamp = QTime(0, 0, 0);

        int secondsToAdd = time.hour() * 3600 + time.minute() * 60 + time.second();
        int msecondsToAdd = time.msec();

        if (negateTime) {
            secondsToAdd = -secondsToAdd;
            msecondsToAdd = -msecondsToAdd;
        }

        currentTimeStamp = currentTimeStamp.addMSecs(msecondsToAdd);
        currentTimeStamp = currentTimeStamp.addSecs(secondsToAdd);

    }

    int blockNumber = textCursor().blockNumber();

    setContent();
    QTextCursor cursor(document()->findBlockByNumber(blockNumber));
    setTextCursor(cursor);
    centerCursor();

    qInfo() << "[Time propagated]"
            << QString("block range: %1 - %2").arg(QString::number(start), QString::number(end))
            << QString("time: %1 %2").arg(negateTime? "-" : "+", time.toString("hh:mm:ss.zzz"));
}

void Editor::selectTags(const QStringList& newTagList)
{
    m_blocks[textCursor().blockNumber()].tagList = newTagList;

    emit refreshTagList(newTagList);

    qInfo() << "[Tags Selected]"
            << "new tags: " << newTagList;
}

void Editor::markWordAsCorrect(int blockNumber, int wordNumber)
{
    auto textToInsert = m_blocks[blockNumber].words[wordNumber].text.toLower();

    if (textToInsert.trimmed() == "")
        return;

    if (std::binary_search(m_dictionary.begin(),
           m_dictionary.end(),
           textToInsert))
    {
        emit message("Word is already correct.");
        return;
    }

    m_dictionary.insert
    (
            std::upper_bound(m_dictionary.begin(), m_dictionary.end(), textToInsert),
            textToInsert
    );

    static_cast<QStringListModel*>(m_textCompleter->model())->setStringList(m_dictionary);
    m_correctedWords.insert(textToInsert);

    QMultiMap<int, int> invalidWords;
    for (int i = 0; i < m_blocks.size(); i++) {
        for (int j = 0; j < m_blocks[i].words.size(); j++) {
            auto wordText = m_blocks[i].words[j].text.toLower();

            if (wordText != "" && m_punctuation.contains(wordText.back()))
                wordText = wordText.left(wordText.size() - 1);

            if (!std::binary_search(m_dictionary.begin(),
                                    m_dictionary.end(),
                                    wordText)
                )
                invalidWords.insert(i, j);
        }
    }
    m_highlighter->setInvalidWords(invalidWords);
    m_highlighter->rehighlight();

    QFile correctedWords(QString("corrected_words_%1.txt").arg(m_transcriptLang));

    if (!correctedWords.open(QFile::WriteOnly | QFile::Truncate))
        emit message("Couldn't write corrected words to file.");
    else {
        for (auto& a_word: m_correctedWords)
            correctedWords.write(QString(a_word + "\n").toStdString().c_str());
    }

    qInfo() << "[Mark As Correct]"
            << QString("text: %1").arg(textToInsert);
}

void Editor::insertSpeakerCompletion(const QString& completion)
{
    if (m_speakerCompleter->widget() != this)
        return;
    QTextCursor tc = textCursor();
    int extra = completion.length() - m_speakerCompleter->completionPrefix().length();

    if (m_speakerCompleter->completionPrefix().length()) {
        tc.movePosition(QTextCursor::Left);
        tc.movePosition(QTextCursor::EndOfWord);
    }

    tc.insertText(completion.right(extra));
    setTextCursor(tc);
}

void Editor::insertTextCompletion(const QString& completion)
{
    if (m_textCompleter->widget() != this)
        return;
    QTextCursor tc = textCursor();
    int extra = completion.length() - m_textCompleter->completionPrefix().length();

    tc.movePosition(QTextCursor::Left);
    tc.movePosition(QTextCursor::EndOfWord);
    tc.insertText(completion.right(extra));

    setTextCursor(tc);
}

void Editor::insertTransliterationCompletion(const QString& completion)
{
    if (m_textCompleter->widget() != this)
        return;

    QTextCursor tc = textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    tc.insertText(completion);

    setTextCursor(tc);
}

void Editor::handleReply()
{
    QStringList tokens;

    QString replyString = m_reply->readAll();

    if (replyString.split("[\"").size() < 4)
        return;

    tokens = replyString.split("[\"")[3].split("]").first().split("\",\"");

    auto lastString = tokens[tokens.size() - 1];
    tokens[tokens.size() - 1] = lastString.left(lastString.size() - 1);

    m_lastReplyList = tokens;
}

void Editor::sendRequest(const QString& input, const QString& langCode)
{
    if (m_reply) {
        m_reply->abort();
        delete m_reply;
        m_reply = nullptr;
    }

    QString url = QString("http://inputtools.google.com/request?text=%1&itc=%2-t-i0-und&num=10&cp=0&cs=1&ie=utf-8&oe=utf-8&app=test").arg(input, langCode);

    QNetworkRequest request(url);
    m_reply = m_manager.get(request);

    connect(m_reply, &QNetworkReply::finished, this, [this] () {
        if (m_reply->error() == QNetworkReply::NoError)
            handleReply();
        else if (m_reply->error() != QNetworkReply::OperationCanceledError)
            emit message(m_reply->errorString());
        emit replyCame();
    });
}
