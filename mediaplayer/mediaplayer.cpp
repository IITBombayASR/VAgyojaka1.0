#include "mediaplayer.h"

MediaPlayer::MediaPlayer(QWidget *parent)
    : QMediaPlayer(parent)
{
}

QTime MediaPlayer::elapsedTime()
{
    return getTimeFromPosition(position());
}

QTime MediaPlayer::durationTime()
{
    return getTimeFromPosition(duration());
}

void MediaPlayer::setPositionToTime(const QTime& time)
{
    if (time.isNull())
        return;
    qint64 position = 3600000*time.hour() + 60000*time.minute() + 1000*time.second() + time.msec();
    setPosition(position);
}

QString MediaPlayer::getMediaFileName()
{
    return m_mediaFileName;
}

QString MediaPlayer::getPositionInfo()
{
    QString format = "mm:ss";
    if (durationTime().hour() != 0)
        format = "hh:mm:ss";
    return elapsedTime().toString(format) + " / " + durationTime().toString(format);
}

void MediaPlayer::open()
{
    QFileDialog fileDialog;
    fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    fileDialog.setWindowTitle(tr("Open Media"));
    QStringList supportedMimeTypes = QMediaPlayer::supportedMimeTypes();
    if (!supportedMimeTypes.isEmpty())
        fileDialog.setMimeTypeFilters(supportedMimeTypes);
    fileDialog.setDirectory(QStandardPaths::standardLocations(QStandardPaths::MoviesLocation).value(0, QDir::homePath()));
    if (fileDialog.exec() == QDialog::Accepted) {
        QUrl *fileUrl = new QUrl(fileDialog.selectedUrls().constFirst());
        m_mediaFileName = fileUrl->fileName();
        setMedia(*fileUrl);
        emit message("Opened file " + fileUrl->fileName());
        play();
    }
}

void MediaPlayer::seek(int seconds)
{
    if (elapsedTime().addSecs(seconds) > durationTime())
        setPosition(duration());
    else if (elapsedTime().addSecs(seconds).isNull())
        setPosition(0);
    else
        setPositionToTime(elapsedTime().addSecs(seconds));
}

QTime MediaPlayer::getTimeFromPosition(const qint64& position)
{
    auto milliseconds = position % 1000;
    auto seconds = (position/1000) % 60;
    auto minutes = (position/60000) % 60;
    auto hours = (position/3600000) % 24;

    return QTime(hours, minutes, seconds, milliseconds);
}

void MediaPlayer::togglePlayback()
{
    if (state() == MediaPlayer::PausedState || state() == MediaPlayer::StoppedState)
        play();
    else if (state() == MediaPlayer::PlayingState)
        pause();
}
