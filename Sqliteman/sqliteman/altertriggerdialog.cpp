/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
	FIXME should update m_name and m_schema after successful alter
		  waiting for new statement parser to fix this
	FIXME trigger with same name as a table confuses it
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
	update(false),
	m_schema(schema),
	m_name(name)
{
	ui.setupUi(this);
	ui.resultEdit->setHtml("");
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
				  + " and type = \"trigger\" ;";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	if (query.lastError().isValid())
	{
		QString errtext = tr("Cannot get trigger ")
						  + name
						  + ":<br/><span style=\" color:#ff0000;\">"
						  + query.lastError().text()
						  + "<br/></span>" + tr("using sql statement:")
						  + "<br/><tt>" + sql;
		resultAppend(errtext);
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
	ui.resultEdit->setHtml("");
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
						  + ":<br/><span style=\" color:#ff0000;\">"
						  + drop.lastError().text()
						  + "<br/></span>" + tr("using sql statement:")
						  + "<br/><tt>" + sql;
		resultAppend(errtext);
		return;
	}

	sql = ui.textEdit->text();
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if(query.lastError().isValid())
	{
		QString errtext = tr("Error while creating trigger ")
						  + m_name
						  + ":<br/><span style=\" color:#ff0000;\">"
						  + query.lastError().text()
						  + "<br/></span>" + tr("using sql statement:")
					  + "<br/><tt>" + sql;
		resultAppend(errtext);
		return;
	}
	resultAppend(tr("Trigger created successfully"));
	update = true;

	// for the moment, we don't allow the user to alter the same trigger again
	// because its name may have changed
	accept();
}

void AlterTriggerDialog::resultAppend(QString text)
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
