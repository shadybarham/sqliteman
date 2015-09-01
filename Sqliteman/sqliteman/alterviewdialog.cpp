/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>
#include <QtDebug>

#include "alterviewdialog.h"
#include "database.h"
#include "utils.h"


AlterViewDialog::AlterViewDialog(const QString & name, const QString & schema,
								 LiteManWindow * parent)
	: QDialog(parent),
	update(false)
{
	ui.setupUi(this);
	ui.databaseCombo->addItem(schema);
	ui.nameEdit->setText(name);
	ui.databaseCombo->setDisabled(true);
	ui.nameEdit->setDisabled(true);

	QString sql = QString("select sql from ")
				  + Utils::quote(schema)
				  + ".sqlite_master where name = "
				  + Utils::quote(name)
				  + ";";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	while (query.next())
	{
		QString s(query.value(0).toString());
		//FIXME parse general CREATE VIEW statement
		int pos = s.indexOf(QRegExp("\\bAS\\b",  Qt::CaseInsensitive));
		if (pos == -1)
		{
			qDebug() << "regexp parse error. Never should be written out";
			ui.createButton->setEnabled(false);
		}
		else
		{
			ui.sqlEdit->setText(s.right(s.length() - pos - 2).trimmed());
			connect(ui.createButton, SIGNAL(clicked()),
					this, SLOT(createButton_clicked()));
		}
	}

	setWindowTitle(tr("Alter View"));
	ui.createButton->setText("&Alter");

	connect(ui.createButton, SIGNAL(clicked()), this, SLOT(createButton_clicked()));
}

void AlterViewDialog::createButton_clicked()
{
	//FIXME this destroys any INSTEAD OF triggers on the view
	//FIXME this destroys the old view before we can be sure that we can create
	// the new one: however there doesn't seem to be any SQL syntax to avoid this, 
	// but maybe we can fix it using a transaction
	//FIXME show old select statement
	//FIXME allow renaming view
	ui.resultEdit->clear();
	QString sql = QString("DROP VIEW ")
				  + Utils::quote(ui.databaseCombo->currentText())
				  + "."
				  + Utils::quote(ui.nameEdit->text())
				  + ";";
	QSqlQuery dropQuery(sql, QSqlDatabase::database(SESSION_NAME));
	if (dropQuery.lastError().isValid())
	{
		QString errtext = QString(tr("Cannot drop view "))
						  + ui.databaseCombo->currentText()
						  + tr(".")
						  + ui.nameEdit->text()
						  + ":\n"
						  + dropQuery.lastError().text()
						  + tr("\nusing sql statement:\n")
						  + sql;
		ui.resultEdit->insertPlainText(errtext);
		return;
	}
	update = true;

	sql = QString("CREATE VIEW ")
		  + Utils::quote(ui.databaseCombo->currentText())
		  + "."
		  + Utils::quote(ui.nameEdit->text())
		  + " AS\n"
		  + ui.sqlEdit->text()
		  + ";";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if(query.lastError().isValid())
	{
		QString errtext = tr("Cannot create view ")
						  + ui.databaseCombo->currentText()
						  + tr(".")
						  + ui.nameEdit->text()
						  + ":\n"
						  + dropQuery.lastError().text()
						  + tr("\nusing sql statement:\n")
						  + sql;
		ui.resultEdit->insertPlainText(errtext);
		ui.resultEdit->moveCursor(QTextCursor::Start);
		return;
	}
	ui.resultEdit->insertPlainText(tr("View altered successfully"));
	ui.resultEdit->insertPlainText("\n");
}
