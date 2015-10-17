/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>
#include <QTreeWidgetItem>
#include <QSettings>

#include "createtriggerdialog.h"
#include "litemanwindow.h"
#include "database.h"
#include "tabletree.h"
#include "utils.h"

CreateTriggerDialog::CreateTriggerDialog(QTreeWidgetItem * item,
										 QWidget * parent)
	: QDialog(parent),
	update(false)
{
	ui.setupUi(this);
	QSettings settings("yarpen.cz", "sqliteman");
	int hh = settings.value("createtrigger/height", QVariant(500)).toInt();
	int ww = settings.value("createtrigger/width", QVariant(600)).toInt();
	resize(ww, hh);

	if (item->type() == TableTree::TableType)
	{
		ui.textEdit->setText(
			QString("-- sqlite3 simple trigger template\n"
					"CREATE TRIGGER [IF NOT EXISTS] ")
			+ Utils::quote(item->text(1))
			+ ".\"<trigger_name>\"\n[ BEFORE | AFTER ]\n"
		    + "DELETE | INSERT | UPDATE | UPDATE OF <column-list>\n ON "
			+ Utils::quote(item->text(0))
			+ "\n[ FOR EACH ROW | FOR EACH STATEMENT ] [ WHEN expression ]\n"
			+ "BEGIN\n<statement-list>\nEND;");
	}
	else // trigger on view
	{
		ui.textEdit->setText(
			QString("-- sqlite3 simple trigger template\n"
					"CREATE TRIGGER [IF NOT EXISTS] ")
			+ Utils::quote(item->text(1))
			+ ".\"<trigger_name>\"\nINSTEAD OF "
			+ "[DELETE | INSERT | UPDATE | UPDATE OF <column-list>]\nON "
			+ Utils::quote(item->text(0))
			+ "\n[ FOR EACH ROW | FOR EACH STATEMENT ] [ WHEN expression ]\n"
			 + "BEGIN\n<statement-list>\nEND;");
	}

	connect(ui.createButton, SIGNAL(clicked()),
			this, SLOT(createButton_clicked()));
}

CreateTriggerDialog::~CreateTriggerDialog()
{
	QSettings settings("yarpen.cz", "sqliteman");
    settings.setValue("createtrigger/height", QVariant(height()));
    settings.setValue("createtrigger/width", QVariant(width()));
}

void CreateTriggerDialog::createButton_clicked()
{
	ui.resultEdit->setHtml("");
	LiteManWindow * lmw = qobject_cast<LiteManWindow*>(parent());
	if (lmw && lmw->checkForPending())
	{
		QString sql(ui.textEdit->text());
		QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
		
		if(query.lastError().isValid())
		{
			resultAppend(tr("Cannot create trigger")
						 + ":<br/><span style=\" color:#ff0000;\">"
						 + query.lastError().text()
						 + "<br/></span>" + tr("using sql statement:")
						 + "<br/><tt>" + sql);
			return;
		}
		resultAppend(tr("Trigger created successfully"));
		update = true;
	}
}

void CreateTriggerDialog::resultAppend(QString text)
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
