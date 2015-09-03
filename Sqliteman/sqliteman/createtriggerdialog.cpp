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

#include "createtriggerdialog.h"
#include "litemanwindow.h"
#include "database.h"
#include "tabletree.h"
#include "utils.h"

CreateTriggerDialog::CreateTriggerDialog(const QString & name,
										 const QString & schema,
										 int itemType,
										 QWidget * parent)
	: QDialog(parent),
	update(false)
{
	ui.setupUi(this);
	QSettings settings("yarpen.cz", "sqliteman");
	int hh = settings.value("createtrigger/height", QVariant(500)).toInt();
	int ww = settings.value("createtrigger/width", QVariant(600)).toInt();
	resize(ww, hh);

	if (itemType == TableTree::TableType)
	{
		ui.textEdit->setText(
			QString("-- sqlite3 simple trigger template\n"
					"CREATE TRIGGER [IF NOT EXISTS] ")
			+ Utils::quote(schema)
			+ ".\"<trigger_name>\"\n[ BEFORE | AFTER ]\n"
		    + "DELETE | INSERT | UPDATE | UPDATE OF <column-list>\n ON "
			+ Utils::quote(name)
			+ "\n[ FOR EACH ROW | FOR EACH STATEMENT ] [ WHEN expression ]\n"
			+ "BEGIN\n<statement-list>\nEND;");
	}
	else
	{
		ui.textEdit->setText(
			QString("-- sqlite3 simple trigger template\n"
					"CREATE TRIGGER [IF NOT EXISTS] ")
			+ Utils::quote(schema)
			+ ".\"<trigger_name>\"\nINSTEAD OF "
			+ "[DELETE | INSERT | UPDATE | UPDATE OF <column-list>]\nON "
			+ Utils::quote(name)
			+ "\n[ FOR EACH ROW | FOR EACH STATEMENT ] [ WHEN expression ]\n"
			 + "BEGIN\n<statement-list>\nEND;");
	}

	connect(ui.createButton, SIGNAL(clicked()), this, SLOT(createButton_clicked()));
}

CreateTriggerDialog::~CreateTriggerDialog()
{
	QSettings settings("yarpen.cz", "sqliteman");
    settings.setValue("createtrigger/height", QVariant(height()));
    settings.setValue("createtrigger/width", QVariant(width()));
}

void CreateTriggerDialog::createButton_clicked()
{
	LiteManWindow * lmw = qobject_cast<LiteManWindow*>(parent());
	if (lmw && lmw->checkForPending())
	{
		QString sql(ui.textEdit->text());
		QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
		
		if(query.lastError().isValid())
		{
			ui.resultEdit->setText(tr("Cannot create trigger:\n")
								   + query.lastError().text()
								   + tr("\nusing sql statement:\n")
								   + sql);
			return;
		}
		ui.resultEdit->setText(tr("Trigger created successfully"));
		update = true;
	}
}
