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

#include "altertriggerdialog.h"
#include "database.h"
#include "utils.h"


AlterTriggerDialog::AlterTriggerDialog(const QString & name, const QString & schema, QWidget * parent)
	: QDialog(parent),
	m_schema(schema),
	m_name(name)
{
	ui.setupUi(this);
	QSettings settings("yarpen.cz", "sqliteman");
	int hh = settings.value("altertrigger/height", QVariant(500)).toInt();
	int ww = settings.value("altertrigger/width", QVariant(600)).toInt();
	resize(ww, hh);
	ui.createButton->setText(tr("&Alter"));
	setWindowTitle("Alter Trigger");

	QString sql = QString("select sql from ")
	              + Utils::quote(schema)
				  + ".sqlite_master where name = "
				  + Utils::quote(name)
				  + ";";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	if (query.lastError().isValid())
	{
		QString errtext = tr("Cannot get trigger ")
						  + name
						  + ":\n"
						  + query.lastError().text()
						  + tr("\nusing sql statement:\n")
						  + sql;
		ui.resultEdit->setText(errtext);
		ui.createButton->setEnabled(false);
	}
	else
	{
		while (query.next())
		{
			ui.textEdit->setText(query.value(0).toString());
			break;
		}
		connect(ui.createButton, SIGNAL(clicked()), this, SLOT(createButton_clicked()));
	}
}

AlterTriggerDialog::~AlterTriggerDialog()
{
	QSettings settings("yarpen.cz", "sqliteman");
    settings.setValue("altertrigger/height", QVariant(height()));
    settings.setValue("altertrigger/width", QVariant(width()));
}

void AlterTriggerDialog::createButton_clicked()
{
	QString sql = QString("DROP TRIGGER ")
				  + Utils::quote(m_schema)
				  + "."
				  + Utils::quote(m_name)
				  + ";";
	QSqlQuery drop(sql, QSqlDatabase::database(SESSION_NAME));
	if(drop.lastError().isValid())
	{
		QString errtext = tr("Cannot drop trigger ")
						  + m_name
						  + ":\n"
						  + drop.lastError().text()
						  + tr("\nusing sql statement:\n")
						  + sql;
		ui.resultEdit->setText(errtext);
		return;
	}

	sql = ui.textEdit->text();
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if(query.lastError().isValid())
	{
		QString errtext = tr("Error while creating trigger ")
						  + m_name
						  + ":\n"
						  + query.lastError().text()
						  + tr("\nusing sql statement:\n")
						  + sql;
		ui.resultEdit->setText(errtext);
		return;
	}
	ui.resultEdit->setText(tr("Trigger created successfully"));
// 	update = true;
}
