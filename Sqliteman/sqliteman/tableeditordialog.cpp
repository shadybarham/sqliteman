/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QTableWidget>
#include <QCheckBox>
#include <QtDebug>
#include <QSettings>

#include "tableeditordialog.h"
#include "utils.h"

TableEditorDialog::TableEditorDialog(QWidget * parent)//, Mode mode, const QString & tableName): QDialog(parent)
{
	ui.setupUi(this);
	
	QSettings settings("yarpen.cz", "sqliteman");
	int hh = settings.value("tableeditor/height", QVariant(600)).toInt();
	int ww = settings.value("tableeditor/width", QVariant(800)).toInt();
	resize(ww, hh);
	ui.tableEditorSplitter->restoreState(settings.value("tableeditor/splitter").toByteArray());
	
	ui.databaseCombo->addItems(Database::getDatabases().keys());

	ui.columnTable->setColumnWidth(0, 150);
	ui.columnTable->setColumnWidth(1, 200);
	connect(ui.nameEdit, SIGNAL(textChanged(const QString&)),
			this, SLOT(nameEdit_textChanged(const QString&)));
	connect(ui.columnTable, SIGNAL(itemSelectionChanged()), this, SLOT(fieldSelected()));
	connect(ui.addButton, SIGNAL(clicked()), this, SLOT(addField()));
	connect(ui.removeButton, SIGNAL(clicked()), this, SLOT(removeField()));
	connect(ui.tabWidget, SIGNAL(currentChanged(int)), this, SLOT(tabWidget_currentChanged(int)));
	connect(ui.createButton, SIGNAL(clicked()), this, SLOT(createButton_clicked()));
	connect(ui.columnTable, SIGNAL(cellChanged(int, int)),
			this, SLOT(tableCellChanged(int, int)));
}

TableEditorDialog::~TableEditorDialog()
{
	QSettings settings("yarpen.cz", "sqliteman");
    settings.setValue("tableeditor/height", QVariant(height()));
    settings.setValue("tableeditor/width", QVariant(width()));
	settings.setValue("tableeditor/splitter", ui.tableEditorSplitter->saveState());
}

void TableEditorDialog::createButton_clicked()
{
	qDebug() << "createButton_clicked() not implemented";
}

void TableEditorDialog::addField()
{
	QComboBox * box = new QComboBox(this);
	QStringList types;
	 
	types << "Text" << "PK Integer"  << "PK Autoincrement" << "Integer" << "Real" << "Blob" << "Null";
	box->setSizeAdjustPolicy(QComboBox::AdjustToContents);

	int rc = ui.columnTable->rowCount();
	ui.columnTable->setRowCount(rc + 1);
	ui.columnTable->setCellWidget(rc, 1, box);
	box->addItems(types);
	ui.columnTable->setCellWidget(rc, 2, new QCheckBox(this));
	nameEdit_textChanged(ui.nameEdit->text());
}

void TableEditorDialog::removeField()
{
	ui.columnTable->removeRow(ui.columnTable->currentRow());
	if (ui.columnTable->rowCount() == 0)
	{
		addField();
	}
	else
	{
		nameEdit_textChanged(ui.nameEdit->text());
	}
}

void TableEditorDialog::fieldSelected()
{
	ui.removeButton->setEnabled(ui.columnTable->selectedRanges().count() != 0);
}

void TableEditorDialog::tableCellChanged(int row, int column)
{
	if (column == 0)
	{
		nameEdit_textChanged(ui.nameEdit->text());
	}
}

void TableEditorDialog::nameEdit_textChanged(const QString& text)
{
	bool bad = text.simplified().isEmpty();
	for(int i = 0; i < ui.columnTable->rowCount(); i++)
	{
		QTableWidgetItem * nameItem = ui.columnTable->item(i, 0);
		if (   (nameItem == 0)
			|| (nameItem->text().isEmpty()))
		{
			bad = true;
		}
	}
	ui.createButton->setDisabled(bad);
}

void TableEditorDialog::tabWidget_currentChanged(int index)
{
	if (index == 1)
		ui.createButton->setEnabled(true);
	else
		nameEdit_textChanged(ui.nameEdit->text());
}

QString TableEditorDialog::getFullName(const QString & objName)
{
	return Utils::quote(ui.databaseCombo->currentText())
		   + "."
		   + Utils::quote(objName);
}

DatabaseTableField TableEditorDialog::getColumn(int row)
{
	DatabaseTableField field;
	QTableWidgetItem * nameItem = ui.columnTable->item(row, 0);

	// skip rows with no column name
	if (nameItem == 0)
	{
		field.cid = -1;
		return field;
	}

	QString type;
	QComboBox * typeBox = qobject_cast<QComboBox *>(ui.columnTable->cellWidget(row, 1));
    if (typeBox)
        type = typeBox->currentText();
    else
        type = ui.columnTable->item(row, 1)->text();

	//}
	bool nn = qobject_cast<QCheckBox*>(ui.columnTable->cellWidget(row, 2))->checkState() == Qt::Checked;

	// For user convenience reasons, the type "INTEGER PRIMARY KEY" is presented to the user
	// as "Primary Key" alone. Therefor, untill there is a more robust solution (which will
	// support translation of type names as well) the primary key type needs to be corrected
	// at update time.
	bool pk = false;
	if (type == "PK Integer")
	{
		type = "Integer Primary Key";
		pk = true;
	}

	if (type == "PK Autoincrement")
	{
		type = "Integer Primary Key Autoincrement";
		pk = true;
	}

	field.cid = 0;
	field.name = nameItem->text();
	field.type = type;
	field.notnull = nn;
	field.defval = (ui.columnTable->item(row, 3) == 0)
				   ? "" : ui.columnTable->item(row, 3)->text();
	field.pk = pk;
	field.comment = "";

	return field;
}

QString TableEditorDialog::getDefaultClause(const QString & defVal)
{
	if (defVal.isNull() || defVal.isEmpty())
		return "";
	bool ok;
	defVal.toDouble(&ok);
	if (ok)
		return QString(" DEFAULT (%1)").arg(defVal);
	else
		return QString(" DEFAULT (%1)").arg(Utils::literal(defVal));
}

QString TableEditorDialog::getColumnClause(DatabaseTableField column)
{
	if (column.cid == -1)
		return QString();

	QString nn(column.notnull ? " NOT NULL" : "");
	QString def(getDefaultClause(column.defval));
	return " " + Utils::quote(column.name)
		       + " " + Utils::literal(column.type) + nn + def + ",\n";
}
