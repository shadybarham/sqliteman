/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>

#include "createviewdialog.h"
#include "database.h"
#include "utils.h"

CreateViewDialog::CreateViewDialog(const QString & name, const QString & schema, LiteManWindow * parent)
	: QDialog(parent),
	update(false)
{
	creator = parent;
	ui.setupUi(this);
	ui.databaseCombo->addItems(Database::getDatabases().keys());

	ui.createButton->setDisabled(true);

	connect(ui.createButton, SIGNAL(clicked()), this, SLOT(createButton_clicked()));
	connect(ui.nameEdit, SIGNAL(textChanged(const QString&)),
			this, SLOT(nameEdit_textChanged(const QString&)));
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
					  + ui.sqlEdit->text()
					  + ";";

		QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
		
		if(query.lastError().isValid())
		{
			ui.resultEdit->setText(tr("Cannot create view:\n")
								   + query.lastError().text()
								   + tr("\nusing sql statement:\n")
								   + sql);
			return;
		}
		ui.resultEdit->setText(tr("View created successfully"));
		update = true;
		m_schema = ui.databaseCombo->currentText();
		m_name = ui.nameEdit->text();
	}
}
