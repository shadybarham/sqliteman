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