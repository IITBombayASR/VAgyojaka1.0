#pragma once

#include <QTableWidget>
#include "blockandword.h"

class WordEditor: public QTableWidget
{
    Q_OBJECT

public:
    explicit WordEditor(QWidget* parent = nullptr);
    QVector<word> currentWords() const;
    void fitTableContents();

public slots:
    void refreshWords(const QVector<word>& words);
    void insertTimeStamp(const QTime& timeToInsert);

private:
    static QTime getTime(const QString& text);
};
