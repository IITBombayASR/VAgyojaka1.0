#pragma once

#include <QDialog>
#include "ui_changespeakerdialog.h"

namespace Ui {
    class ChangeSpeakerDialog;
}

class ChangeSpeakerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChangeSpeakerDialog(QWidget* parent = nullptr): QDialog(parent), ui(new Ui::ChangeSpeakerDialog)
    {
        ui->setupUi(this);
    }

    ~ChangeSpeakerDialog() {delete ui;}

    QString speaker() const
    {
        return ui->comboBox_speaker->currentText();
    }
    
    bool replaceAll() const
    {
        return ui->checkBox_changeAllOccurences->isChecked();
    }
    
    void addItems(const QStringList& speakers) const
    {
        ui->comboBox_speaker->addItems(speakers);
    }

    void setCurrentSpeaker(const QString& speakerName) const
    {
        ui->label_currentSpeaker->setText(speakerName);
    }

private:
    Ui::ChangeSpeakerDialog* ui;
};
