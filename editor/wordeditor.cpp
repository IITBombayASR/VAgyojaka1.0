#include "wordeditor.h"

#include <QHeaderView>

WordEditor::WordEditor(QWidget* parent)
    : QTableWidget(parent)
{
    setColumnCount(4);

    setHorizontalHeaderItem(0, new QTableWidgetItem("Text"));
    setHorizontalHeaderItem(1, new QTableWidgetItem("End Time"));
    setHorizontalHeaderItem(2, new QTableWidgetItem("InvW"));
    setHorizontalHeaderItem(3, new QTableWidgetItem("Slacked"));

    fitTableContents();
}

QVector<word> WordEditor::currentWords() const
{
    QVector<word> wordsToReturn;

    for (int i = 0; i < rowCount(); i++) {
        auto text = item(i, 0)->text();
        auto timeStamp = getTime(item(i, 1)->text());
        QStringList tagList;

        if (item(i, 2)->checkState() == Qt::Checked)
            tagList << "InvW";
        if (item(i, 3)->checkState() == Qt::Checked)
            tagList << "Slacked";

        wordsToReturn.append(word {timeStamp, text, tagList});
    }

    return wordsToReturn;
}

void WordEditor::refreshWords(const QVector<word>& words)
{
    clear();

    setHorizontalHeaderItem(0, new QTableWidgetItem("Text"));
    setHorizontalHeaderItem(1, new QTableWidgetItem("End Time"));
    setHorizontalHeaderItem(2, new QTableWidgetItem("InvW"));
    setHorizontalHeaderItem(3, new QTableWidgetItem("Slacked"));

    if (words.isEmpty())
        return;

    setRowCount(words.size());

    int counter = 0;
    for (auto& a_word: words) {
        auto text = a_word.text;
        auto timeStamp = a_word.timeStamp;
        auto tagList = a_word.tagList;

        setItem(counter, 0, new QTableWidgetItem(text));
        setItem(counter, 1, new QTableWidgetItem(timeStamp.toString("hh:mm:ss.zzz")));
        setItem(counter, 2, new QTableWidgetItem);
        setItem(counter, 3, new QTableWidgetItem);

        if (tagList.contains("InvW"))
            item(counter, 2)->setCheckState(Qt::Checked);
        else
            item(counter, 2)->setCheckState(Qt::Unchecked);

        if (tagList.contains("Slacked"))
            item(counter, 3)->setCheckState(Qt::Checked);
        else
            item(counter, 3)->setCheckState(Qt::Unchecked);

        counter++;
    }

    fitTableContents();
}

void WordEditor::insertTimeStamp(const QTime& timeToInsert)
{
    item(currentRow(), 1)->setText(timeToInsert.toString("hh:mm:ss.zzz"));
}

void WordEditor::fitTableContents()
{
    resizeRowsToContents();
    resizeColumnsToContents();
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

QTime WordEditor::getTime(const QString& text)
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


