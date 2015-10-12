/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
	FIXME allow CREATE VIEW to use Query Builder
*/

#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>
#include <QSettings>

#include "createviewdialog.h"
#include "database.h"
#include "utils.h"

CreateViewDialog::CreateViewDialog(const QString & name, const QString & schema, LiteManWindow * parent)
	: QDialog(parent),
	update(false)
{
	creator = parent;
	ui.setupUi(this);
	QSettings settings("yarpen.cz", "sqliteman");
	int hh = settings.value("createview/height", QVariant(500)).toInt();
	int ww = settings.value("createview/width", QVariant(600)).toInt();
	resize(ww, hh);

	ui.databaseCombo->addItems(Database::getDatabases().keys());

	ui.createButton->setDisabled(true);

	connect(ui.createButton, SIGNAL(clicked()), this, SLOT(createButton_clicked()));
	connect(ui.nameEdit, SIGNAL(textChanged(const QString&)),
			this, SLOT(nameEdit_textChanged(const QString&)));
}

CreateViewDialog::~CreateViewDialog()
{
	QSettings settings("yarpen.cz", "sqliteman");
    settings.setValue("createview/height", QVariant(height()));
    settings.setValue("createview/width", QVariant(width()));
}

void CreateViewDialog::nameEdit_textChanged(const QString& text)
{
	ui.createButton->setDisabled(text.simplified().isEmpty());
}


void CreateViewDialog::createButton_clicked()
{
	ui.resultEdit->setHtml("");
	if (creator && creator->checkForPending())
	{
		QString sql = QString("CREATE VIEW ")
					  + Utils::quote(ui.databaseCombo->currentText())
					  + "."
					  + Utils::quote(ui.nameEdit->text())
					  + " AS "
					  + ui.sqlEdit->text();
		if (!sql.endsWith(";")) { sql = sql + ";"; }

		QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
		
		if(query.lastError().isValid())
		{
			resultAppend(tr("Cannot create view")
						 + ":<br/><span style=\" color:#ff0000;\">"
						 + query.lastError().text()
						 + "<br/></span>" + tr("using sql statement:")
						 + "<br/><tt>" + sql);
			return;
		}
		resultAppend(tr("View created successfully"));
		update = true;
		m_schema = ui.databaseCombo->currentText();
		m_name = ui.nameEdit->text();
	}
}

void CreateViewDialog::resultAppend(QString text)
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
