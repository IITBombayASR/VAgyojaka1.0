#pragma once

#include <QDialog>
#include <QTime>
#include "ui_timepropagationdialog.h"

namespace Ui {
    class TimePropagationDialog;
}

class TimePropagationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TimePropagationDialog(QWidget* parent = nullptr): QDialog(parent), ui(new Ui::TimePropagationDialog)
    {
        ui->setupUi(this);
        ui->spinBox_h->setRange(0, 23);
        ui->spinBox_m->setRange(0, 59);
        ui->spinBox_s->setRange(0,59.999);
    }

    ~TimePropagationDialog() {delete ui;}

    QTime time() const
    {
        int ms = (ui->spinBox_s->value() - (long)ui->spinBox_s->value()) * 1000;
        return QTime(ui->spinBox_h->value(),
                     ui->spinBox_m->value(),
                     ui->spinBox_s->value(),
                     ms);
    }

    void setBlockRange(int currentBlockNumber, int end)
    {
        ui->spinBox_end->setRange(0, end);
        ui->spinBox_end->setValue(end);
        ui->spinBox_start->setValue(currentBlockNumber);
    }

    int blockStart() const
    {
        return ui->spinBox_start->value();
    }

    int blockEnd() const
    {
        return ui->spinBox_end->value();
    }

    bool negateTime() const
    {
        return ui->checkBox_negateTime->isChecked();
    }

private:
    Ui::TimePropagationDialog* ui;
};
