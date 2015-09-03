/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>
#include <QSettings>

#include "constraintsdialog.h"
#include "database.h"
#include "utils.h"

ConstraintsDialog::ConstraintsDialog(const QString & tabName, const QString & schema, QWidget * parent)
	: QDialog(parent),
	m_schema(schema),
	m_table(tabName)
{
	update = false;
	ui.setupUi(this);
	QSettings settings("yarpen.cz", "sqliteman");
	int hh = settings.value("constraints/height", QVariant(500)).toInt();
	int ww = settings.value("constraints/width", QVariant(600)).toInt();
	resize(ww, hh);

	// trigger name templates
	ui.insertName->setText(QString("tr_cons_%1_ins").arg(tabName));
	ui.updateName->setText(QString("tr_cons_%1_upd").arg(tabName));
	ui.deleteName->setText(QString("tr_cons_%1_del").arg(tabName));

	// not nulls
	QStringList inserts;
	QStringList updates;
	QStringList deletes;
	QStringList nnCols;
	QString stmt;
	foreach (DatabaseTableField column, Database::tableFields(tabName, schema))
	{
		if (!column.notnull)
			continue;
		nnCols << column.name;
		stmt = QString("SELECT RAISE(ABORT, 'New ")
			   + column.name
			   + " value IS NULL') WHERE new."
			   + Utils::quote(column.name)
			   + " IS NULL;\n";
		inserts << "-- NOT NULL check" << stmt;
		updates << "-- NOT NULL check"<< stmt;
	}

	// get FKs
	QString sql = QString("pragma ")
				  + Utils::quote(schema)
				  + ".foreign_key_list ("
				  + Utils::quote(tabName)
				  + ");";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if(query.lastError().isValid())
	{
		QString errtext = QString(tr("Cannot parse constraints:\n"))
						  + query.lastError().text()
						  + tr("\nusing sql statement:\n")
						  + sql;
		ui.resultEdit->setText(errtext);
		return;
	}
	// 2 - table - FK table; 3 - from - column name; 4 - to - fk column name
	QString fkTab;
	QString column;
	QString fkColumn;
	QString nnTemplate;
	QString thenTemplate;
	while (query.next())
	{
		fkTab = query.value(2).toString();
		column = query.value(3).toString();
		fkColumn = query.value(4).toString();
		nnTemplate = "";
		if (nnCols.contains(column, Qt::CaseInsensitive))
		{
			nnTemplate = QString("\n    new.%1 IS NOT NULL AND")
								 .arg(Utils::quote(column));
		}
		thenTemplate = QString("\n    RAISE(ABORT, '")
					   + column
					   + " violates foreign key "
					   + fkTab
					   + "("
					   + fkColumn
					   + ")')";
		stmt = QString("SELECT ")
			   + thenTemplate
			   + "\n    where "
			   + nnTemplate
			   + " SELECT "
			   + Utils::quote(fkColumn)
			   + " FROM "
			   + Utils::quote(fkTab)
			   + " WHERE "
			   + Utils::quote(fkColumn)
			   + " = new."
			   + Utils::quote(column)
			   + ") IS NULL;\n";
		inserts << "-- FK check" << stmt;
		updates << "-- FK check" << stmt;
		stmt = QString("SELECT ")
			   + thenTemplate
			   + " WHERE (SELECT "
			   + Utils::quote(fkColumn)
			   + " FROM "
			   + Utils::quote(fkTab)
			   + " WHERE "
			   + Utils::quote(fkColumn)
			   + " = old."
			   + Utils::quote(column)
			   + ") IS NOT NULL;\n";
		deletes << "-- FK check" << stmt;
	}

	// to the GUI
	ui.insertEdit->setText(inserts.join("\n"));
	ui.updateEdit->setText(updates.join("\n"));
	ui.deleteEdit->setText(deletes.join("\n"));

	connect(ui.createButton, SIGNAL(clicked()), this, SLOT(createButton_clicked()));
}

ConstraintsDialog::~ConstraintsDialog()
{
	QSettings settings("yarpen.cz", "sqliteman");
    settings.setValue("constraints/height", QVariant(height()));
    settings.setValue("constraints/width", QVariant(width()));
}

void ConstraintsDialog::createButton_clicked()
{
	QString status("INSERT trigger\n");
	status += execSql("BEGIN TRANSACTION;", tr("Cannot begin transaction:\n"));
	if (ui.insertEdit->text().length() != 0)
	{
		status += execSql(QString("CREATE TRIGGER ")
						  + Utils::quote(ui.insertName->text())
						  + " BEFORE INSERT ON "
						  + Utils::quote(m_schema)
						  + "."
						  + Utils::quote(m_table)
						  + " FOR EACH ROW\nBEGIN\n"
						  + "-- created by Sqliteman tool\n\n"
						  + ui.insertEdit->text()
						  + "END;",
						  tr("Cannot create trigger:\n"));
	}
	else
	{
		status += tr("No action for INSERT");
	}

	status += "\nUPDATE trigger\n";
	if (ui.updateEdit->text().length() != 0)
	{
		status += execSql(QString("CREATE TRIGGER ")
						  + Utils::quote(ui.updateName->text())
						  + " BEFORE UPDATE ON "
						  + Utils::quote(m_schema)
						  + "."
						  + Utils::quote(m_table)
						  + " FOR EACH ROW\nBEGIN\n"
						  + "-- created by Sqliteman tool\n\n"
						  + ui.updateEdit->text()
						  + "END;",
						  tr("Cannot create trigger:\n"));
	}
	else
		status += tr("No action for UPDATE");

	status += "\nDELETE trigger\n";
	if (ui.deleteEdit->text().length() != 0)
	{
		status += execSql(QString("CREATE TRIGGER ")
						  + Utils::quote(ui.updateName->text())
						  + " BEFORE DELETE ON "
						  + Utils::quote(m_schema)
						  + "."
						  + Utils::quote(m_table)
						  + " FOR EACH ROW\nBEGIN\n"
						  + "-- created by Sqliteman tool\n\n"
						  + ui.deleteEdit->text()
						  + "END;",
						  tr("Cannot create trigger:\n"));
	}
	else
		status += tr("No action for DELETE");
	status += execSql("COMMIT;", tr("Cannot commit transaction:\n"));
	ui.resultEdit->setText(status);
}

QString ConstraintsDialog::execSql(const QString & statement,
								   const QString & message)
{
	QSqlQuery query(statement, QSqlDatabase::database(SESSION_NAME));
	if (query.lastError().isValid())
	{
		return message
			   + query.lastError().text()
			   + tr("\nusing sql statement:\n")
			   + statement;
	}
	else
	{
		update = true;
		return tr("Trigger created successfully");
	}
}
