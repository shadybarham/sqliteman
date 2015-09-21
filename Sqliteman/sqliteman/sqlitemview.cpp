/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QDataWidgetMapper>
#include <QGridLayout>
#include <QSqlRecord>
#include <QTextEdit>
#include <QSqlQueryModel>
#include <QTableView>

#include "sqlitemview.h"
#include "sqlmodels.h"
#include "database.h"

SqlItemView::SqlItemView(QWidget * parent)
	: QWidget(parent),
	m_row(0),
	m_model(0)
{
	setupUi(this);

	connect(firstButton, SIGNAL(clicked()),
			this, SLOT(toFirst()));
	connect(previousButton, SIGNAL(clicked()),
			this, SLOT(toPrevious()));

	connect(nextButton, SIGNAL(clicked()),
			this, SLOT(toNext()));
	connect(lastButton, SIGNAL(clicked()),
			this, SLOT(toLast()));
	connect(qApp, SIGNAL(focusChanged(QWidget*,QWidget*)),
			 this, SLOT(aApp_focusChanged(QWidget*,QWidget*)));
}

void SqlItemView::setModel(QAbstractItemModel * model)
{
	m_model = model;
	QSqlQueryModel * t = qobject_cast<QSqlQueryModel *>(model);
	if (!t)  { return; }
	QSqlRecord rec(t->record());

	if (scrollWidget->widget())
	{
		delete scrollWidget->takeWidget();
	}

	QWidget * layoutWidget = new QWidget(scrollWidget);
	m_gridLayout = new QGridLayout(layoutWidget);
	QString tmp("%1:");

	for (int i = 0; i < rec.count(); ++i)
	{
		m_gridLayout->addWidget(
			new QLabel(tmp.arg(rec.fieldName(i)), layoutWidget), i, 0);
		QTextEdit * w = new QTextEdit(layoutWidget);
		w->setReadOnly(true); // TODO: make it transaction reliable
		w->setAcceptRichText(false);
		int mh = QFontMetrics(w->currentFont()).lineSpacing();
		w->setMinimumHeight(mh);
		w->setSizePolicy(QSizePolicy::Expanding,
                         QSizePolicy::MinimumExpanding);
		m_gridLayout->addWidget(w, i, 1);
		m_gridLayout->setRowMinimumHeight(i, mh);
	}
	scrollWidget->setWidget(layoutWidget);

	m_count = rec.count();
}

QAbstractItemModel * SqlItemView::model()
{
	return m_model;
}

void SqlItemView::updateButtons(int row)
{
	int rowcount = m_model->rowCount();
	int notDeleted = 0;
	int adjRow = row;
	for (int i = 0; i < rowcount; ++i)
	{
		if (m_table->isRowHidden(i))
		{
			if (i < row) { --adjRow; }
		}
		else { ++notDeleted; }
	}
	positionLabel->setText(tr("%1 of %2").arg(adjRow + 1).arg(notDeleted));
	previousButton->setEnabled(findDown(row) != row);
	firstButton->setEnabled(findUp(-1) != row);
	nextButton->setEnabled(findUp(row) != row);
	lastButton->setEnabled(findDown(rowcount) != row);
	copyButton->setEnabled(qobject_cast<SqlTableModel *>(m_model) != 0);
}

void SqlItemView::setCurrentIndex(int row, int column)
{
	m_column = column;
	// We need to do this even if m_row has not changed, in case we're selecting
	// row 0 for the first time after the item viewer was created
	m_row = row;
	for (int i = 0; i < m_count; ++i)
	{
		QWidget * w = m_gridLayout->itemAtPosition(i, 1)->widget();
		QTextEdit * te = qobject_cast<QTextEdit *>(w);
		if (te)
		{
			QColor color =
				m_model->data(m_model->index(row, i),
							  Qt::BackgroundColorRole).value<QColor>();
			QPalette p(te->palette());
			p.setColor(QPalette::Active, QPalette::Base, color);
			p.setColor(QPalette::Inactive, QPalette::Base, color);
			te->setPalette(p);
			QVariant rawdata = m_model->data(
				m_model->index(row, i), Qt::EditRole);
			if (rawdata.type() == QVariant::ByteArray)
			{
				rawdata = m_model->data(
					m_model->index(row, i), Qt::DisplayRole);
				te->setReadOnly(true);
			}
			if (i == column)
			{
				te->setText(rawdata.toString());
			}
			else
			{
				te->setText(m_model->data(
					m_model->index(row, i), Qt::DisplayRole).toString());
			}
		}
	}
	updateButtons(row);
}

int SqlItemView::currentRow()
{
	return m_row;
}

int SqlItemView::currentColumn()
{
	return m_column;
}

int SqlItemView::findUp(int row)
{
	int r = row;
	while (++r < m_model->rowCount())
	{
		if (m_table->isRowHidden(r)) { continue; }
		else { return r; }
	}
	return row;
}

int SqlItemView::findDown(int row)
{
	int r = row;
	while (--r >= 0)
	{
		if (m_table->isRowHidden(r)) { continue; }
		else { return r; }
	}
	return row;
}

void SqlItemView::toFirst()
{
	int row = findUp(-1);
	if (row != m_row) { setCurrentIndex(row, m_column); }
}

void SqlItemView::toPrevious()
{
	int row = findDown(m_row);
	if (row != m_row) { setCurrentIndex(row, m_column); }
}

void SqlItemView::toNext()
{
	int row = findUp(m_row);
	if (row != m_row) { setCurrentIndex(row, m_column); }
}

void SqlItemView::toLast()
{
	int row = findDown(m_model->rowCount());
	if (row != m_row) { setCurrentIndex(row, m_column); }
}

void SqlItemView::aApp_focusChanged(QWidget* old, QWidget* now)
{
	if (m_model)
	{
		for (int i = 0; i < m_count; ++i)
		{
			QWidget * w = m_gridLayout->itemAtPosition(i, 1)->widget();
			if (w == now)
			{
				m_column = i;
				setCurrentIndex(m_row, m_column);
				emit indexChanged();
				break;
			}
		}
	}
}
