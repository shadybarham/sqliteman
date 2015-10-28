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
#include <QMessageBox>
#include <QLineEdit>

#include "createviewdialog.h"
#include "database.h"
#include "utils.h"

CreateViewDialog::CreateViewDialog(LiteManWindow * parent,
									 QTreeWidgetItem * item)
	: TableEditorDialog(parent)
{
	creator = parent;
	updated = false;
	m_dirty = false;
	ui.labelTable->setText("View Name:");
	setWindowTitle(tr("Create View"));

	if (item)
	{
		int i = ui.databaseCombo->findText(item->text(1),
			Qt::MatchFixedString | Qt::MatchCaseSensitive);
		if (i >= 0)
		{
			ui.databaseCombo->setCurrentIndex(i);
			ui.databaseCombo->setDisabled(true);
		}
	}

	ui.tabWidget->setCurrentIndex(1);
	ui.tabWidget->removeTab(0);
	m_tabWidgetIndex = ui.tabWidget->currentIndex();
	connect(ui.tabWidget, SIGNAL(currentChanged(int)),
			this, SLOT(tabWidget_currentChanged(int)));
	m_createButton =
		ui.buttonBox->addButton("Create", QDialogButtonBox::ApplyRole);
	m_createButton->setDisabled(true);

	connect(m_createButton, SIGNAL(clicked()), this, SLOT(createButton_clicked()));
	connect(ui.nameEdit, SIGNAL(textChanged(const QString&)),
			this, SLOT(checkChanges()));

	ui.textEdit->setText("");
	ui.queryEditor->setItem(0);
}

void CreateViewDialog::setText(QString query)
{
	ui.textEdit->setText(query);
	setDirty();
}

QString CreateViewDialog::getSQLfromGUI()
{
	QString sql;
	switch (m_tabWidgetIndex)
	{
		case 0:
			sql = getFullName("VIEW");
			sql += " AS " + ui.queryEditor->statement();
			break;
		case 1:
			sql = ui.textEdit->text();
	}
	return sql;
}

void CreateViewDialog::createButton_clicked()
{
	ui.resultEdit->setHtml("");
	if (creator && creator->checkForPending())
	{
		QString sql(getSQLfromGUI());

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
		updated = true;
		emit rebuildViewTree(ui.databaseCombo->currentText(),
							 ui.nameEdit->text());
		resultAppend(tr("View created successfully"));
	}
}

bool CreateViewDialog::checkRetained(int i)
{
	return true;
}

bool CreateViewDialog::checkColumn(int i, QString cname,
								   QString ctype, QString cextra)
{
	return false;
}

void CreateViewDialog::checkChanges()
{
	m_createButton->setEnabled(!ui.nameEdit->text().trimmed().isEmpty());
}

void CreateViewDialog::setDirty()
{
	m_dirty = true;
}

void CreateViewDialog::tabWidget_currentChanged(int index)
{
	if (index == 1)
	{
		if (m_dirty && (m_tabWidgetIndex != 1))
		{
			
			int com = QMessageBox::question(this, tr("Sqliteman"),
				tr("Do you want to keep the current content of the SQL editor?."
				   "\nYes to keep it,\nNo to create from the %1 tab"
				   "\nCancel to return to the %2 tab")
				.arg("AS query", "AS query"),
				QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);
			if (com == QMessageBox::No)
				ui.textEdit->setText(getSQLfromGUI());
			else if (com == QMessageBox::Cancel)
			{
				ui.tabWidget->setCurrentIndex(m_tabWidgetIndex);
				return;
			}
		}
		else
		{
			ui.textEdit->setText(getSQLfromGUI());
			setDirty();
		}
		ui.labelDatabase->hide();
		ui.databaseCombo->hide();
		ui.labelTable->hide();
		ui.nameEdit->hide();
		ui.adviceLabel->hide();
		m_tabWidgetIndex = index;
		m_createButton->setEnabled(true);
	}
	else
	{
		ui.labelDatabase->show();
		ui.databaseCombo->show();
		ui.labelTable->show();
		ui.nameEdit->show();
		ui.adviceLabel->show();
		m_tabWidgetIndex = index;
		checkChanges();
	}
}
