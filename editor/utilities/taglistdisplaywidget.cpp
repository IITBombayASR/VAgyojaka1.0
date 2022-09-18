#include "taglistdisplaywidget.h"

#include <QHeaderView>

TagListDisplayWidget::TagListDisplayWidget(QWidget* parent)
	: QTableWidget(parent)
{
    horizontalHeader()->hide();
    verticalHeader()->hide();
    setEditTriggers(QAbstractItemView::NoEditTriggers);

    setFixedHeight(fontMetrics().height());
}

void TagListDisplayWidget::refreshTags(const QStringList &tagList)
{
    clear();

    setColumnCount(tagList.size());
    setRowCount(1);

    if (tagList.isEmpty())
        return;

    for (int i = 0; i < tagList.size(); i++)
        setItem(0, i, new QTableWidgetItem(QString("  %1  ").arg(tagList[i])));

    resizeRowsToContents();
    resizeColumnsToContents();

    setFixedHeight(rowHeight(0));
}