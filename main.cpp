#include "tool.h"

#include <QApplication>

void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(context);

    QString dateTime = QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss");
    QString dateTimeText = QString("[%1] ").arg(dateTime);

    QByteArray localMsg = msg.toLocal8Bit();
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";

    switch (type) {
    case QtDebugMsg:
        fprintf(stderr, "Debug: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtInfoMsg:
        fprintf(stderr, "Info: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtWarningMsg:
        fprintf(stderr, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtCriticalMsg:
        fprintf(stderr, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtFatalMsg:
        fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    }

    QFile outFile("LogFile.log");
    outFile.open(QIODevice::WriteOnly | QIODevice::Append);

    QTextStream textStream(&outFile);
    textStream.setCodec("UTF-8");

    QHash<QtMsgType, QString> msgLevelHash({{QtDebugMsg, "Debug:"},
                                            {QtInfoMsg, "Info:"},
                                            {QtWarningMsg, "Warning:"},
                                            {QtCriticalMsg, "Critical:"},
                                            {QtFatalMsg, "Fatal:"}}
                                           );
    QString logLevelName = msgLevelHash[type];
    QString logText = QString("%1 %2 %3 (%4)").arg(dateTimeText, logLevelName, msg,  context.file);

    textStream << logText << "\n";
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(customMessageHandler);
    QApplication a(argc, argv);

    Tool w;
    w.show();

    return a.exec();
}
