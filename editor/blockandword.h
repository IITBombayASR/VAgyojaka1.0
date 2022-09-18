#pragma once

#include <QVector>
#include <QTime>

struct word
{
    QTime timeStamp;
    QString text;
    QStringList tagList;

    inline bool operator==(word w) const
    {
        if (w.timeStamp == timeStamp && w.text == text)
            return true;
        return false;
    }
};

struct block
{
    QTime timeStamp;
    QString text;
    QString speaker;
    QStringList tagList;
    QVector<word> words;

    inline bool operator==(block b) const
    {
        if(b.timeStamp==timeStamp && b.text==text && b.speaker==speaker && b.words==words)
            return true;
        return false;
    }
};
