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
#include <QLineEdit>

#include "tableeditordialog.h"
#include "utils.h"
#include "preferences.h"

TableEditorDialog::TableEditorDialog(QWidget * parent)//, Mode mode, const QString & tableName): QDialog(parent)
{
	ui.setupUi(this);
	
	QSettings settings("yarpen.cz", "sqliteman");
	int hh = settings.value("tableeditor/height", QVariant(600)).toInt();
	int ww = settings.value("tableeditor/width", QVariant(800)).toInt();
	resize(ww, hh);
	
	ui.databaseCombo->addItems(Database::getDatabases().keys());

	ui.columnTable->setColumnWidth(0, 150);
	ui.columnTable->setColumnWidth(1, 200);
	connect(ui.columnTable, SIGNAL(itemSelectionChanged()),
			this, SLOT(fieldSelected()));
	connect(ui.addButton, SIGNAL(clicked()), this, SLOT(addField()));
	connect(ui.removeButton, SIGNAL(clicked()), this, SLOT(removeField()));
}

TableEditorDialog::~TableEditorDialog()
{
	QSettings settings("yarpen.cz", "sqliteman");
    settings.setValue("tableeditor/height", QVariant(height()));
    settings.setValue("tableeditor/width", QVariant(width()));
}

void TableEditorDialog::addField()
{
	Preferences * prefs = Preferences::instance();
	bool useNull = prefs->nullHighlight();
	QString nullText = prefs->nullHighlightText();

	int rc = ui.columnTable->rowCount();
	ui.columnTable->setRowCount(rc + 1);

	QLineEdit * name = new QLineEdit(this);
	name->setFrame(false);
	connect(name, SIGNAL(textEdited(const QString &)),
			this, SLOT(checkChanges()));
	ui.columnTable->setCellWidget(rc, 0, name);

	QComboBox * box = new QComboBox(this);
	QStringList types;
	if (useNull && !nullText.isEmpty()) { types << nullText; }
	else { types << "Null"; }
	types << "Text" << "PK Integer"  << "PK Autoincrement"
		  << "Integer" << "Real" << "Blob";
	box->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	box->addItems(types);
	box->setEditable(true);
	ui.columnTable->setCellWidget(rc, 1, box);

	ui.columnTable->setCellWidget(rc, 2, new QCheckBox(this));

	QLineEdit * defval = new QLineEdit(this);
	if (useNull)
	{
		defval->setPlaceholderText(nullText);
	}
	defval->setFrame(false);
	connect(defval, SIGNAL(textEdited(const QString &)),
			this, SLOT(checkChanges()));
	ui.columnTable->setCellWidget(rc, 3, defval);

	checkChanges();
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
		checkChanges();
	}
}

void TableEditorDialog::fieldSelected()
{
	ui.removeButton->setEnabled(ui.columnTable->selectedRanges().count() != 0);
}

QString TableEditorDialog::getFullName(const QString & objName)
{
	return Utils::quote(ui.databaseCombo->currentText())
		   + "."
		   + Utils::quote(objName);
}

DatabaseTableField TableEditorDialog::getColumn(int row)
{
	Preferences * prefs = Preferences::instance();
	bool useNull = prefs->nullHighlight();
	QString nullText = prefs->nullHighlightText();

	DatabaseTableField field;
	QLineEdit * nameItem =
		qobject_cast<QLineEdit *>(ui.columnTable->cellWidget(row, 0));

	QString type;
	QComboBox * box =
		qobject_cast<QComboBox *>(ui.columnTable->cellWidget(row, 1));
	type = box->currentText();
	if (useNull && !nullText.isEmpty())
	{
		if (type == nullText) { type = QString(""); }
	}
	else
	{
		if (type == "Null") { type = QString(""); }
	}

	QCheckBox * cb =
		qobject_cast<QCheckBox*>(ui.columnTable->cellWidget(row, 2));
	bool nn = cb->checkState() == Qt::Checked;

	QLineEdit * defBox =
		qobject_cast<QLineEdit *>(ui.columnTable->cellWidget(row, 3));

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
	field.defval = defBox->text();
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
	QString nn(column.notnull ? " NOT NULL" : "");
	QString tt = column.type.isEmpty() ? "" : Utils::literal(column.type);
	QString def(getDefaultClause(column.defval));
	return " " + Utils::quote(column.name)
		       + " " + tt + nn + def + ",\n";
}

void TableEditorDialog::resultAppend(QString text)
{
	ui.resultEdit->append(text);
	int lh = QFontMetrics(ui.resultEdit->currentFont()).lineSpacing();
	QTextDocument * doc = ui.resultEdit->document();
	if (doc)
	{
		int h = (int)(doc->size().height());
		if (h < lh * 2) { h = lh * 2 + lh / 2; }
		ui.resultEdit->setFixedHeight(h + lh / 2);
	}
	else
	{
		int lines = text.split("<br/>").count() + 1;
		ui.resultEdit->setFixedHeight(lh * lines);
	}
}
