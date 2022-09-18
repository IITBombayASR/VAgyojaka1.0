#pragma once

#include <QTableWidget>
#include <QStringList>

class TagListDisplayWidget: public QTableWidget
{
	Q_OBJECT

public:
	explicit TagListDisplayWidget(QWidget* parent = nullptr);

public slots:
    void refreshTags(const QStringList& tagList);
};
