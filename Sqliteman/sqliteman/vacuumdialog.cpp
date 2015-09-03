/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QSettings>

#include "vacuumdialog.h"
#include "database.h"
#include "utils.h"

VacuumDialog::VacuumDialog(LiteManWindow * parent)
	: QDialog(parent)
{
	creator = parent;
	ui.setupUi(this);
	QSettings settings("yarpen.cz", "sqliteman");
	int hh = settings.value("vacuum/height", QVariant(500)).toInt();
	int ww = settings.value("vacuum/width", QVariant(600)).toInt();
	resize(ww, hh);

	ui.tableList->addItems(Database::getObjects("table").values());
	ui.tableList->addItems(Database::getObjects("index").values());

	connect(ui.allButton, SIGNAL(clicked()), this, SLOT(allButton_clicked()));
	connect(ui.tableButton, SIGNAL(clicked()), this, SLOT(tableButton_clicked()));
}

VacuumDialog::~VacuumDialog()
{
	QSettings settings("yarpen.cz", "sqliteman");
    settings.setValue("vacuum/height", QVariant(height()));
    settings.setValue("vacuum/width", QVariant(width()));
}

void VacuumDialog::allButton_clicked()
{
	if (creator && creator->checkForPending())
	{
		Database::execSql("vacuum;");
	}
}

void VacuumDialog::tableButton_clicked()
{
	QList<QListWidgetItem *> list(ui.tableList->selectedItems());
	if (creator && creator->checkForPending())
	{
		for (int i = 0; i < list.size(); ++i)
		{
			if (!Database::execSql(QString("vacuum %1;")
								   .arg(Utils::quote(list.at(i)->text()))))
				break;
		}
	}
}

