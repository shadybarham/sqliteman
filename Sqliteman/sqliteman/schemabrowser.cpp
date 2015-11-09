/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QInputDialog>
#include <QFileInfo>

#include "schemabrowser.h"
#include "database.h"
#include "extensionmodel.h"


SchemaBrowser::SchemaBrowser(QWidget * parent, Qt::WindowFlags f)
	: QWidget(parent, f)
{
	setupUi(this);

	m_extensionModel = new ExtensionModel(this);
	extensionTableView->setModel(m_extensionModel);

#ifndef ENABLE_EXTENSIONS
	schemaTabWidget->setTabEnabled(2, false);
#endif

// 	connect(pragmaTable, SIGNAL(currentCellChanged(int, int, int, int)),
// 			this, SLOT(pragmaTable_currentCellChanged(int, int, int, int)));
	connect(setPragmaButton, SIGNAL(clicked()), this, SLOT(setPragmaButton_clicked()));
}

void SchemaBrowser::buildPragmasTree()
{
	int row = pragmaTable->currentRow();
	disconnect(pragmaTable, SIGNAL(currentCellChanged(int, int, int, int)),
			   this, SLOT(pragmaTable_currentCellChanged(int, int, int, int)));
	pragmaTable->clearContents();
	pragmaTable->setRowCount(0);

	addPragma("application_id");
	addPragma("auto_vacuum");
	addPragma("automatic_index");
	addPragma("busy_timeout");
	addPragma("cache_size");
	addPragma("cache_spill");
	addPragma("case_sensitive_like");
	addPragma("cell_size_check");
	addPragma("checkpoint_fullfsync");
	addPragma("data_version");
	addPragma("default_cache_size");
	addPragma("default_synchronous");
	addPragma("encoding");
	addPragma("foreign_keys");
	addPragma("fullfsync");
	addPragma("ignore_check_constraints");
	addPragma("journal_mode");
	addPragma("journal_size_limit");
	addPragma("legacy_file_format");
	addPragma("locking_mode");
	addPragma("max_page_count");
	addPragma("mmap_size");
	addPragma("page_size");
	addPragma("query_only");
	addPragma("read_uncommitted");
	addPragma("recursive_triggers");
	addPragma("reverse_unordered_selects");
	addPragma("secure_delete");
	addPragma("short_column_names");
	addPragma("soft_heap_limit");
	addPragma("synchronous");
	addPragma("temp_store");
	addPragma("threads");
	addPragma("user_version");
	addPragma("wal_autocheckpoint");

	if (row < 0) { row = 0; }
	pragmaTable_currentCellChanged(row, 0, 0, 0);
	pragmaTable->setCurrentItem(pragmaTable->item(row, 0));
	connect(pragmaTable, SIGNAL(currentCellChanged(int, int, int, int)),
		    this, SLOT(pragmaTable_currentCellChanged(int, int, int, int)));
}

void SchemaBrowser::addPragma(const QString & name)
{
	int row = pragmaTable->rowCount();
	pragmaTable->setRowCount(row + 1);
	pragmaTable->setItem(row, 0, new QTableWidgetItem(name));
	pragmaTable->setItem(row, 1, new QTableWidgetItem(Database::pragma(name)));
}

void SchemaBrowser::pragmaTable_currentCellChanged(int currentRow, int /*currentColumn*/, int /*previousRow*/, int /*previousColumn*/)
{
	pragmaName->setText(pragmaTable->item(currentRow, 0)->text());
	pragmaValue->setText(pragmaTable->item(currentRow, 1)->text());
}

void SchemaBrowser::setPragmaButton_clicked()
{
	Database::execSql(QString("PRAGMA main.")
					  + pragmaName->text()
					  + " = "
					  + pragmaValue->text()
					  + ";");
	buildPragmasTree();
}

void SchemaBrowser::appendExtensions(const QStringList & list, bool switchToTab)
{
	QStringList l(m_extensionModel->extensions());
	QString s;
	foreach (s, list)
	{
		l.removeAll(s);
		l.append(s);
	}
	m_extensionModel->setExtensions(l);
	extensionTableView->resizeColumnsToContents();

	if (switchToTab)
		schemaTabWidget->setCurrentIndex(2);
}
