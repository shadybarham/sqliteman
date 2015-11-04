/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QScrollBar>
#include <QComboBox>
#include <QLineEdit>
#include <QSettings>
#include <QTreeWidgetItem>

#include "queryeditordialog.h"
#include "queryeditorwidget.h"

QueryEditorDialog::QueryEditorDialog(QWidget * parent): QDialog(parent)
{
	setupUi(this);
	QSettings settings("yarpen.cz", "sqliteman");
	int hh = settings.value("queryeditor/height", QVariant(500)).toInt();
	int ww = settings.value("queryeditor/width", QVariant(600)).toInt();
	resize(ww, hh);
}

void QueryEditorDialog::setItem(QTreeWidgetItem * item)
{
	queryEditor->setItem(item);
}

QueryEditorDialog::~QueryEditorDialog()
{
	QSettings settings("yarpen.cz", "sqliteman");
    settings.setValue("queryeditor/height", QVariant(height()));
    settings.setValue("queryeditor/width", QVariant(width()));
}

QString QueryEditorDialog::statement()
{
	return queryEditor->statement();
}

void QueryEditorDialog::treeChanged()
{
	queryEditor->treeChanged();
}

void QueryEditorDialog::tableAltered(QString oldName, QTreeWidgetItem * item)
{
	queryEditor->tableAltered(oldName, item);
}
