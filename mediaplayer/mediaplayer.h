#pragma once

#include <QWidget>
#include <QMediaPlayer>
#include <QFileDialog>
#include <QStandardPaths>
#include <QTime>

class MediaPlayer : public QMediaPlayer
{
    Q_OBJECT
public:
    explicit MediaPlayer(QWidget *parent = nullptr);
    QTime elapsedTime();
    QTime durationTime();
    void setPositionToTime(const QTime& time);
    QString getMediaFileName();
    QString getPositionInfo();

public slots:
    void open();
    void seek(int seconds);
    void togglePlayback();

signals:
    void message(QString text, int timeout = 5000);

private:
    static QTime getTimeFromPosition(const qint64& position);
    QString m_mediaFileName;
};
