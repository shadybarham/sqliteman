/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.

This file written by and copyright © 2018 Richard P. Parkins, M. A., and released under the GPL.
*/

#include "sqltableview.h"

// override QTableview::sizeHintForColumn
int SqlTableView::sizeHintForColumn(int column) const
{
    if (!model())
        return -1;

    ensurePolished();

    int rows = verticalHeader()->count();

    QStyleOptionViewItem option = viewOptions();

    float total = 0;
    QModelIndex index;
    for (int row = 0; row <= rows; ++row) {
        int logicalRow = verticalHeader()->logicalIndex(row);
        if (verticalHeader()->isSectionHidden(logicalRow))
            continue;
        index = model()->index(logicalRow, column);
		total += itemDelegate(index)->sizeHint(option, index).width();
    }
    total += (showGrid() ? 2 * rows : rows) - 1;

    return (int) (total / rows);
}

// override QTableview::sizeHintForRow
int SqlTableView::sizeHintForRow(int row) const
{
#if 0
    if (!model())
        return -1;

    ensurePolished();

    int columns = horizontalHeader()->count();

    QStyleOptionViewItem option = viewOptions();

	int height = 0;
    QModelIndex index;
    for (int col = 0; col <= columns; ++col) {
        int logicalColumn = horizontalHeader()->logicalIndex(col);
        if (verticalHeader()->isSectionHidden(logicalColumn))
            continue;
        index = model()->index(row, logicalColumn);
        option.rect.setY(rowViewportPosition(index.row()));
        option.rect.setHeight(rowHeight(index.row()));
        option.rect.setX(columnViewportPosition(index.column()));
        option.rect.setWidth(columnWidth(index.column()));
		int h = itemDelegate(index)->sizeHint(option, index).height();
		if (height < h) { height = h; }
    }
    if(showGrid()) { ++height; }

    return height;
#endif
	return QTableView::sizeHintForRow(row);
}
