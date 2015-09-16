/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
	FIXME allow CREATE TABLE to use Query Builder
*/

#include <QCheckBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QLineEdit>

#include "createtabledialog.h"
#include "database.h"
#include "utils.h"

CreateTableDialog::CreateTableDialog(LiteManWindow * parent)
	: TableEditorDialog(parent)
{
	creator = parent;
	updated = false;
	ui.removeButton->setEnabled(false); // Disable row removal
	addField(); // A table should have at least one field
	setWindowTitle(tr("Create Table"));

	ui.createButton->setDisabled(true);
	connect(ui.nameEdit, SIGNAL(textEdited(const QString&)),
			this, SLOT(checkChanges()));

	ui.textEdit->setText(
		"CREATE TABLE [IF NOT EXISTS] <database-name.table-name>\n\
(\n\
    <column-name> <type> <constraint...>,\n\
    ...\n\
    [, constraints ]\n\
)");
}

QString CreateTableDialog::getSQLfromGUI()
{
	QString sql = QString("CREATE TABLE ")
				  + Utils::quote(ui.databaseCombo->currentText())
				  + "."
				  + Utils::quote(ui.nameEdit->text())
				  + " (\n";
	QString nn;
	QString def;
	DatabaseTableField f;

	for(int i = 0; i < ui.columnTable->rowCount(); i++)
	{
		f = getColumn(i);
		sql += getColumnClause(f);
	}
	sql = sql.remove(sql.size() - 2, 2); 	// cut the extra ", "
	sql += "\n);\n";

	return sql;
}

void CreateTableDialog::createButton_clicked()
{
	if (creator && creator->checkForPending())
	{
		QString sql;
		// from GUI
		if (ui.tabWidget->currentIndex() == 0)
			sql = getSQLfromGUI();
		else
			sql = ui.textEdit->text();

		QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
		if(query.lastError().isValid())
		{
			ui.resultEdit->setHtml(tr("Cannot create table")
								   + ":<br/><span style=\" color:#ff0000;\">"
								   + query.lastError().text()
								   + "<br/></span>" + tr("using sql statement:")
								   + "<br/><tt>" + sql);
			return;
		}
		updated = true;
		ui.resultEdit->setHtml(tr("Table created successfully"));
	}
}

void CreateTableDialog::checkChanges()
{
	bool bad = ui.nameEdit->text().trimmed().isEmpty();
	for(int i = 0; i < ui.columnTable->rowCount(); i++)
	{
		QLineEdit * name =
			qobject_cast<QLineEdit*>(ui.columnTable->cellWidget(i, 0));
		if (   (name == 0)
			|| (name->text().trimmed().isEmpty()))
		{
			bad = true;
		}
	}
	ui.createButton->setDisabled(bad);
}

void CreateTableDialog::tabWidget_currentChanged(int index)
{
	if (index == 1)
	{
		//FIXME only do this if editor window is dirty
		// and use more sensible message
		int com = QMessageBox::question(this, tr("Sqliteman"),
			tr("The current content of the Advanced SQL editor will be lost."
			   "Do you really want to recreate your SQL?"),
			QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);
		if (com == QMessageBox::Yes)
			ui.textEdit->setText(getSQLfromGUI());
		else if (com == QMessageBox::Cancel)
			ui.tabWidget->setCurrentIndex(0);
	}
	TableEditorDialog::tabWidget_currentChanged(index);
}
