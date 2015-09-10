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
			ui.resultEdit->setText(tr("Cannot create view")
								   + ":<br/><span style=\" color:#ff0000;\">"
								   + query.lastError().text()
								   + "<br/></span>" + tr("using sql statement:")
								   + "<br/><tt>" + sql);
			return;
		}
		ui.resultEdit->setText(tr("View created successfully"));
		update = true;
		m_schema = ui.databaseCombo->currentText();
		m_name = ui.nameEdit->text();
	}
}
