/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
	FIXME allow CREATE TABLE to use Query Builder
	FIXME Context create table should use correct database
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
	m_dirty = false;
	ui.removeButton->setEnabled(false); // Disable row removal
	addField(); // A table should have at least one field
	setWindowTitle(tr("Create Table"));

	ui.createButton->setDisabled(true);
	connect(ui.nameEdit, SIGNAL(textEdited(const QString&)),
			this, SLOT(checkChanges()));

	ui.textEdit->setText("");
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
	m_dirty = true;

	return sql;
}

void CreateTableDialog::createButton_clicked()
{
	ui.resultEdit->setHtml("");
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
			resultAppend(tr("Cannot create table")
						 + ":<br/><span style=\" color:#ff0000;\">"
						 + query.lastError().text()
						 + "<br/></span>" + tr("using sql statement:")
						 + "<br/><tt>" + sql);
			return;
		}
		updated = true;
		resultAppend(tr("Table created successfully"));
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

void CreateTableDialog::setDirty()
{
	m_dirty = true;
}

void CreateTableDialog::tabWidget_currentChanged(int index)
{
	if (index == 1)
	{
		if (m_dirty)
		{
			int com = QMessageBox::question(this, tr("Sqliteman"),
				tr("Do you want to keep the current content of the SQL editor?."
				   "\nYes to keep it,\nNo to create from the Design tab"
				   "\nCancel to return to the Design tab"),
				QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);
			if (com == QMessageBox::No)
				ui.textEdit->setText(getSQLfromGUI());
			else if (com == QMessageBox::Cancel)
				ui.tabWidget->setCurrentIndex(0);
		}
		else
		{
			ui.textEdit->setText(getSQLfromGUI());
		}
	}
	TableEditorDialog::tabWidget_currentChanged(index);
}
