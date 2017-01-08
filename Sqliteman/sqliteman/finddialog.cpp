/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QComboBox>
#include <QLineEdit>
#include <QScrollBar>
#include <QSettings>
#include <QSqlRecord>
#include <QTreeWidgetItem>

#include "database.h"
#include "dataviewer.h"
#include "finddialog.h"
#include "sqlparser.h"
#include "ui_finddialog.h"
#include "ui_termstabwidget.h"
#include "utils.h"

bool FindDialog::notSame(QStringList l1, QStringList l2)
{
	QStringList l1s = QStringList(l1);
	QStringList l2s = QStringList(l2);
	l1s.sort();
	l2s.sort();
	return l1s != l2s;
}

void FindDialog::closeEvent(QCloseEvent * event)
{
	emit findClosed();
	QMainWindow::closeEvent(event);
}

FindDialog::FindDialog(QWidget * parent)
{
	setupUi(this);
	connect(finishedbutton, SIGNAL(clicked()), this, SLOT(close()));
	QSettings settings("yarpen.cz", "sqliteman");
	int hh = settings.value("finddialog/height", QVariant(400)).toInt();
	int ww = settings.value("finddialog/width", QVariant(600)).toInt();
	resize(ww, hh);
}

FindDialog::~FindDialog()
{
	QSettings settings("yarpen.cz", "sqliteman");
    settings.setValue("finddialog/height", QVariant(height()));
    settings.setValue("finddialog/width", QVariant(width()));
}

void FindDialog::updateButtons()
{
	bool enabled = termsTab->termsTable->rowCount() != 0;
	findfirst->setEnabled(enabled);
	findnext->setEnabled(enabled);
	findall->setEnabled(enabled);
}

void FindDialog::doConnections(DataViewer * dataviewer)
{
	connect(findfirst, SIGNAL(clicked()),
			dataviewer, SLOT(findFirst()));
	connect(findnext, SIGNAL(clicked()),
			dataviewer, SLOT(findNext()));
	connect(findall, SIGNAL(clicked()),
			dataviewer, SLOT(findAll()));
	connect(this, SIGNAL(findClosed()),
			dataviewer, SLOT(findClosing()));
	connect(termsTab->termMoreButton, SIGNAL(clicked()),
			this, SLOT(updateButtons()));
	connect(termsTab->termLessButton, SIGNAL(clicked()),
			this, SLOT(updateButtons()));
}

void FindDialog::setup(QString schema, QString table)
{
	setWindowTitle(QString("Find in table ") + schema + "." + table);
	QStringList columns;
	SqlParser * parser = Database::parseTable(table, schema);
	foreach (FieldInfo i, parser->m_fields) { columns << i.name; }
	delete parser;
	if (   (schema != m_schema)
		|| (table != m_table)
		|| (notSame(termsTab->m_columnList, columns)))
	{
		m_schema = schema;
		m_table = table;
		termsTab->m_columnList = columns;
		termsTab->termsTable->clear();
		termsTab->termsTable->setRowCount(0);
	}
	updateButtons();
}

bool FindDialog::isMatch(QSqlRecord * rec, int i)
{
	QComboBox * field = qobject_cast<QComboBox *>
		(termsTab->termsTable->cellWidget(i, 0));
	QComboBox * relation = qobject_cast<QComboBox *>
		(termsTab->termsTable->cellWidget(i, 1));
	QLineEdit * value = qobject_cast<QLineEdit *>
		(termsTab->termsTable->cellWidget(i, 2));
	if (field && relation)
	{
		QVariant data(rec->value(field->currentText()));
		switch (relation->currentIndex())
		{
			case 0:	// Contains
				return  data.toString().contains(value->text());

			case 1:	// Doesn't contain
				return !(data.toString().contains(value->text()));
			
			case 2:	// Starts with
				return data.toString().startsWith(value->text());

			case 3:	// Equals
				return data.toString() == value->text();

			case 4:	// Not equals
				return data.toString() != value->text();

			case 5:	// Bigger than
				return data.toString() > value->text();

			case 6:	// Smaller than
				return data.toString() < value->text();

			case 7:	// is null
				return data.isNull();

			case 8:	// is not null
				return !(data.isNull());
		}
	}
	return false;
}

bool FindDialog::isMatch(QSqlRecord * rec)
{
	int terms = termsTab->termsTable->rowCount();
	if (terms > 0)
	{
		if (termsTab->andButton->isChecked())
		{
			for (int i = 0; i < terms; ++i)
			{
				if (!isMatch(rec, i)) { return false; }
			}
			return true;
		}
		else // or button must be checked
		{
			for (int i = 0; i < terms; ++i)
			{
				if (isMatch(rec, i)) { return true; }
			}
			return false;
		}
	}
	else
	{
		return true; // empty term list matches anything
	}
}
