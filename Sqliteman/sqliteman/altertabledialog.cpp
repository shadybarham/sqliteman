/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QCheckBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QTreeWidgetItem>

#include "altertabledialog.h"
#include "utils.h"


AlterTableDialog::AlterTableDialog(LiteManWindow * parent,
								   QTreeWidgetItem * item,
								   const bool isActive)
	: TableEditorDialog(parent),
	m_item(item),
	m_protectedRows(0),
	m_dropColumns(0)
{
	creator = parent;
	m_alteringActive = isActive;
	updateStage = 0;

	ui.nameEdit->setText(m_item->text(0));
// 	ui.nameEdit->setDisabled(true);
	ui.databaseCombo->addItem(m_item->text(1));
	ui.databaseCombo->setDisabled(true);
	ui.tabWidget->removeTab(1);
	ui.adviceLabel->hide();
	ui.createButton->setText(tr("Alte&r"));
	ui.removeButton->setEnabled(false);
	setWindowTitle(tr("Alter Table"));

	ui.columnTable->insertColumn(4); // show if it's indexed
	QTableWidgetItem * captIx = new QTableWidgetItem(tr("Indexed"));
	ui.columnTable->setHorizontalHeaderItem(4, captIx);
	ui.columnTable->insertColumn(5); // drop protected columns
	QTableWidgetItem * captDrop = new QTableWidgetItem(tr("Drop"));
	ui.columnTable->setHorizontalHeaderItem(5, captDrop);

	connect(ui.columnTable, SIGNAL(cellClicked(int, int)), this, SLOT(cellClicked(int,int)));

	resetStructure();
}

void AlterTableDialog::resetStructure()
{
	// obtain all indexed colums for DROP COLUMN checks
	foreach(QString index,
		Database::getObjects("index", m_item->text(1)).values(m_item->text(0)))
	{
		foreach(QString indexColumn,
			Database::indexFields(index, m_item->text(1)))
		{
			m_columnIndexMap[indexColumn].append(index);
		}
	}

	// Initialize fields
	FieldList fields = Database::tableFields(m_item->text(0), m_item->text(1));
	ui.columnTable->clearContents();
	ui.columnTable->setRowCount(fields.size());
	for(int i = 0; i < fields.size(); i++)
	{
		QTableWidgetItem * nameItem = new QTableWidgetItem(fields[i].name);
		QTableWidgetItem * typeItem = new QTableWidgetItem(fields[i].type);
// 		typeItem->setFlags(Qt::ItemIsSelectable); // TODO: change afinity in ALTER TABLE too!
		QTableWidgetItem * defItem = new QTableWidgetItem(fields[i].defval);
		QTableWidgetItem * ixItem = new QTableWidgetItem();

		QCheckBox * dropItem = new QCheckBox(this);
		connect(dropItem, SIGNAL(stateChanged(int)),
				this, SLOT(dropItem_stateChanged(int)));

		ixItem->setFlags(Qt::ItemIsSelectable);
		if (m_columnIndexMap.contains(fields[i].name))
		{
			ixItem->setIcon(Utils::getIcon("index.png"));
			ixItem->setText(tr("Yes"));
		}
		else
			ixItem->setText(tr("No"));

		ui.columnTable->setItem(i, 0, nameItem);
		ui.columnTable->setItem(i, 1, typeItem);
		QCheckBox *nn = new QCheckBox(this);
		nn->setCheckState(fields[i].notnull ? Qt::Checked : Qt::Unchecked);
		ui.columnTable->setCellWidget(i, 2, nn);
		ui.columnTable->setItem(i, 3, defItem);
		ui.columnTable->setItem(i, 4, ixItem);
		ui.columnTable->setCellWidget(i, 5, dropItem);
	}

	m_protectedRows = ui.columnTable->rowCount();
	m_dropColumns = 0;
// 	ui.columnTable->resizeColumnsToContents();
	checkChanges();
}

void AlterTableDialog::dropItem_stateChanged(int state)
{
	state == Qt::Checked ? ++m_dropColumns : --m_dropColumns;
	checkChanges();
}

bool AlterTableDialog::execSql(const QString & statement, const QString & message, const QString & tmpName)
{
	QSqlQuery query(statement, QSqlDatabase::database(SESSION_NAME));
	if(query.lastError().isValid())
	{
		QString errtext = message
						  + ":\n"
						  + query.lastError().text()
						  + tr("\nusing sql statement:\n")
						  + statement;
		ui.resultEdit->append(errtext);
		if (!tmpName.isNull())
			ui.resultEdit->append(tr("Old table is stored as %1").arg(tmpName));
		return false;
	}
	return true;
}

QStringList AlterTableDialog::originalSource()
{
	QString ixsql = QString("select sql from ")
					+ Utils::quote(m_item->text(1))
					+ ".sqlite_master where type in ('index', 'trigger') "
					+ "and tbl_name = "
					+ Utils::quote(m_item->text(0))
					+ ";";
	QSqlQuery query(ixsql, QSqlDatabase::database(SESSION_NAME));
	QStringList ret;

	if (query.lastError().isValid())
	{
		QString errtext = tr("Cannot get index list for ")
						  + m_item->text(0)
						  + ":\n"
						  + query.lastError().text()
						  + tr("\nusing sql statement:\n")
						  + ixsql;
		ui.resultEdit->append(errtext);
		return QStringList();
	}
	while(query.next())
		ret.append(query.value(0).toString());
	return ret;
}

bool AlterTableDialog::renameTable()
{
	QString newTableName(ui.nameEdit->text().trimmed());
	if ((!m_alteringActive) || (creator && creator->checkForPending()))
	{
		if (m_item->text(0) == newTableName) { return true; }
	
		QString sql = QString("ALTER TABLE ")
					  + Utils::quote(m_item->text(1))
					  + "."
					  + Utils::quote(m_item->text(0))
					  + " RENAME TO "
					  + Utils::quote(newTableName)
					  + ";";
		QString message = tr("Cannot rename table ")
						  + m_item->text(0)
						  + tr(" to ")
						  + newTableName;
		if (execSql(sql, message))
		{
			updateStage = 1;
			m_item->setText(0, newTableName);
			return true;
		}
	}
	return false;
}

void AlterTableDialog::createButton_clicked()
{
	ui.resultEdit->clear();
	// rename table if it's required
	if (!renameTable())
		return;

	// drop columns first
// 	if (m_dropColumns > 0)
	{
        FieldList oldColumns =
	        Database::tableFields(m_item->text(0), m_item->text(1));
		QStringList existingObjects = Database::getObjects().keys();
		// indexes and triggers on the original table
		QStringList originalSrc = originalSource();

		// generate unique temporary tablename
		QString tmpName("_alter%1_" + m_item->text(0));
		int tmpCount = 0;
		while (existingObjects.contains(tmpName.arg(tmpCount), Qt::CaseInsensitive))
			++tmpCount;
		tmpName = tmpName.arg(tmpCount);

		// create temporary table without selected columns
		FieldList newColumns;
        QStringList tmpSelectColumns;
		for(int i = 0; i < m_protectedRows; ++i)
		{
			if (!qobject_cast<QCheckBox*>(ui.columnTable->cellWidget(i, 5))->isChecked())
            {
				newColumns.append(getColumn(i));
                tmpSelectColumns.append(oldColumns[i].name);
            }
		}

		QString sql = QString("ALTER TABLE ")
					  + Utils::quote(m_item->text(1))
					  + "."
					  + Utils::quote(m_item->text(0))
					  + " RENAME TO "
					  + Utils::quote(tmpName)
					  + ";";
		QString message = tr("Cannot rename table ")
						  + m_item->text(0)
						  + tr(" to ")
						  + tmpName;
		if (!execSql(sql, message))
		{
			return;
		}
		updateStage = 1;

		sql = QString("CREATE TABLE ")
			  + Utils::quote(m_item->text(1))
			  + "."
			  + Utils::quote(m_item->text(0))
			  + " (";
		QStringList tmpInsertColumns;
		foreach (DatabaseTableField f, newColumns)
		{
			sql += getColumnClause(f);
			tmpInsertColumns.append(f.name);
		}
		sql = sql.remove(sql.size() - 2, 2); // cut the extra ", "
		sql += "\n);\n";

		message = tr("Cannot create table ")
				  + m_item->text(0);
		if (!execSql(sql, message))
		{
			return;
		}

		// insert old data
		if (!execSql("BEGIN TRANSACTION;", tr("Cannot begin transaction"), tmpName))
		{
			return;
		}
		sql = QString("INSERT INTO ")
			  + Utils::quote(m_item->text(1))
			  + "."
			  + Utils::quote(m_item->text(0))
			  + " ("
			  + Utils::quote(tmpInsertColumns)
			  + ") SELECT "
			  + Utils::quote(tmpSelectColumns)
			  + " FROM "
			  + Utils::quote(m_item->text(1))
			  + "."
			  + Utils::quote(tmpName)
			  + ";";
		message = tr("Cannot insert data into ")
				  + tmpName;
		if (!execSql(sql, message))
			return;
		if (!execSql("COMMIT;", tr("Cannot commit"), tmpName))
			return;
		updateStage = 2;
		
		// drop old table
		sql = QString("DROP TABLE ")
			  + Utils::quote(m_item->text(1))
			  + "."
			  + Utils::quote(tmpName)
			  + ";";
		message = tr("Cannot drop table ")
				  + tmpName;
		if (!execSql(sql, message))
			return;

		// restoring original indexes
		foreach (QString restoreSql, originalSrc)
			execSql(restoreSql, tr("Cannot recreate original index/trigger"));
	}

	// handle add columns
	addColumns();

	if (updateStage > 0)
	{
		resetStructure();
		ui.resultEdit->append(tr("Alter Table Done"));
	}
}

bool AlterTableDialog::addColumns()
{
	// handle new columns
	DatabaseTableField f;
	QString sql = QString("ALTER TABLE ")
				  + Utils::quote(ui.databaseCombo->currentText())
				  + "."
				  + Utils::quote(m_item->text(0))
				  + " ADD COLUMN ";
	QString fullSql;

	// only if it's required to do
	if (m_protectedRows == ui.columnTable->rowCount())
		return true;
	
	for(int i = m_protectedRows; i < ui.columnTable->rowCount(); i++)
	{
		f = getColumn(i);
		if (f.cid == -1)
			continue;

		fullSql = sql
				  + Utils::quote(f.name)
				  + f.type
				  + (f.notnull ? " NOT NULL" : "")
				  + getDefaultClause(f.defval)
				  + ";";

		QSqlQuery query(fullSql, QSqlDatabase::database(SESSION_NAME));
		if(query.lastError().isValid())
		{
			QString errtext = tr("Cannot add column ")
							  + f.name
							  + tr(" to ")
							  + m_item->text(0)
							  + ":\n"
							  + query.lastError().text()
							  + tr("\nusing sql statement:\n")
							  + fullSql;
			ui.resultEdit->append(errtext);
			return false;
		}
		updateStage = 2;
	}
	ui.resultEdit->append(tr("Columns added successfully"));
	return true;
}

void AlterTableDialog::addField()
{
	TableEditorDialog::addField();
	checkChanges();
}

void AlterTableDialog::removeField()
{
	if (ui.columnTable->currentRow() < m_protectedRows)
		return;
	TableEditorDialog::removeField();
	checkChanges();
}

void AlterTableDialog::fieldSelected()
{
	if (ui.columnTable->currentRow() < m_protectedRows)
	{
		ui.removeButton->setEnabled(false);
		return;
	}
	TableEditorDialog::fieldSelected();
}

void AlterTableDialog::cellClicked(int row, int)
{
	if (row < m_protectedRows)
	{
		ui.removeButton->setEnabled(false);
		return;
	}
	TableEditorDialog::fieldSelected();
}

void AlterTableDialog::checkChanges()
{
// 	ui.createButton->setEnabled(m_dropColumns > 0 || m_protectedRows < ui.columnTable->rowCount());
	ui.createButton->setEnabled(true);
}
