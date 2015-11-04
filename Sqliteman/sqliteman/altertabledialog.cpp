/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
	FIXME allow reordering columns
	FIXME loses some constraints
	mulptiple primary key are not allowed
	multiple not null are allowed
	multiple unique are allowed if any on conflict clauses are the same
	multiple different checks are allowed
	multiple default are possible, last one is used
	multiple collate are possible even with different collation names
*/

#include <QCheckBox>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QTreeWidgetItem>

#include "altertabledialog.h"
#include "litemanwindow.h"
#include "utils.h"


AlterTableDialog::AlterTableDialog(LiteManWindow * parent,
								   QTreeWidgetItem * item,
								   const bool isActive)
	: TableEditorDialog(parent),
	m_item(item)
{
	m_alteringActive = isActive;
	ui.removeButton->setEnabled(false);
	setWindowTitle(tr("Alter Table"));
	m_tableOrView = "TABLE";
	m_dubious = false;
	m_oldWidget = ui.designTab;

	m_alterButton =
		ui.buttonBox->addButton("Alte&r", QDialogButtonBox::ApplyRole);
	m_alterButton->setDisabled(true);
	connect(m_alterButton, SIGNAL(clicked(bool)),
			this, SLOT(alterButton_clicked()));

	// item must be valid and a table, otherwise we don't get called
	ui.nameEdit->setText(m_item->text(0));
	int i = ui.databaseCombo->findText(m_item->text(1),
		Qt::MatchFixedString | Qt::MatchCaseSensitive);
	if (i >= 0)
	{
		ui.databaseCombo->setCurrentIndex(i);
		ui.databaseCombo->setDisabled(true);
	}

	ui.tabWidget->removeTab(2);
	ui.tabWidget->removeTab(1);
	m_tabWidgetIndex = ui.tabWidget->currentIndex();
	ui.adviceLabel->hide();

	ui.columnTable->insertColumn(4); // show if it's indexed
	QTableWidgetItem * captIx = new QTableWidgetItem(tr("Indexed"));
	ui.columnTable->setHorizontalHeaderItem(4, captIx);
	ui.columnTable->insertColumn(5); // drop protected columns
	QTableWidgetItem * captDrop = new QTableWidgetItem(tr("Drop"));
	ui.columnTable->setHorizontalHeaderItem(5, captDrop);

	connect(ui.columnTable, SIGNAL(cellClicked(int, int)),
			this, SLOT(cellClicked(int,int)));

	resetStructure();
}

void AlterTableDialog::resetStructure()
{
	// obtain all indexed columns for DROP COLUMN checks
	QMap<QString,QStringList> columnIndexMap;
	foreach(QString index,
		Database::getObjects("index", m_item->text(1)).values(m_item->text(0)))
	{
		foreach(QString indexColumn,
			Database::indexFields(index, m_item->text(1)))
		{
			columnIndexMap[indexColumn].append(index);
		}
	}

	// Initialize fields
	SqlParser parsed = Database::parseTable(m_item->text(0), m_item->text(1));
	m_fields = parsed.m_fields;
	ui.columnTable->clearContents();
	ui.columnTable->setRowCount(0);
	for (int i = 0; i < m_fields.size(); i++)
	{
		int x = m_fields[i].isAutoIncrement ? 3
				: ( m_fields[i].isPartOfPrimaryKey ? 2
					: (m_fields[i].isNotNull ? 1 : 0));

		TableEditorDialog::addField(m_fields[i].name, m_fields[i].type, x,
									SqlParser::defaultToken(m_fields[i]));

		QTableWidgetItem * ixItem = new QTableWidgetItem();
		ixItem->setFlags(Qt::NoItemFlags);
		if (columnIndexMap.contains(m_fields[i].name))
		{
			ixItem->setIcon(Utils::getIcon("index.png"));
			ixItem->setText(tr("Yes"));
		}
		else { ixItem->setText(tr("No")); }
		ui.columnTable->setItem(i, 4, ixItem);

		QCheckBox * dropItem = new QCheckBox(this);
		connect(dropItem, SIGNAL(stateChanged(int)),
				this, SLOT(checkChanges()));
		ui.columnTable->setCellWidget(i, 5, dropItem);
	}

	m_keptColumns.clear();
	m_hadRowid = parsed.m_hasRowid;
	ui.withoutRowid->setChecked(!m_hadRowid);

	checkChanges();
}

bool AlterTableDialog::execSql(const QString & statement,
							   const QString & message)
{
	QSqlQuery query(statement, QSqlDatabase::database(SESSION_NAME));
	if(query.lastError().isValid())
	{
		QString errtext = message
						  + ":<br/><span style=\" color:#ff0000;\">"
						  + query.lastError().text()
						  + "<br/></span>" + tr("using sql statement:")
						  + "<br/><tt>" + statement;
		resultAppend(errtext);
		return false;
	}
	return true;
}

bool AlterTableDialog::doRollback(QString message)
{
	bool result = execSql("ROLLBACK TO ALTER_TABLE;", message);
	if (result)
	{
		// rollback does not cancel the savepoint
		if (execSql("RELEASE ALTER_TABLE;", QString("")))
		{
			return true;
		}
	}
	resultAppend(tr("Database may be left with a pending savepoint."));
	return result;
}

QStringList AlterTableDialog::originalSource(QString tableName)
{
	QString ixsql = QString("select sql from ")
					+ Database::getMaster(m_item->text(1))
					+ " where type in ('index', 'trigger') "
					+ "and tbl_name = "
					+ Utils::quote(tableName)
					+ ";";
	QSqlQuery query(ixsql, QSqlDatabase::database(SESSION_NAME));

	if (query.lastError().isValid())
	{
		QString errtext = tr("Cannot get index list for ")
						  + m_item->text(0)
						  + ":<br/><span style=\" color:#ff0000;\">"
						  + query.lastError().text()
						  + "<br/></span>" + tr("using sql statement:")
						  + "<br/><tt>" + ixsql;
		resultAppend(errtext);
		return QStringList();
	}
	QStringList ret;
	while (query.next())
		{ ret.append(query.value(0).toString()); }
	return ret;
}

bool AlterTableDialog::renameTable(QString oldTableName, QString newTableName)
{
	QString sql = QString("ALTER TABLE ")
				  + Utils::quote(m_item->text(1))
				  + "."
				  + Utils::quote(oldTableName)
				  + " RENAME TO "
				  + Utils::quote(newTableName)
				  + ";";
	QString message = tr("Cannot rename table ")
					  + oldTableName
					  + tr(" to ")
					  + newTableName;
	return execSql(sql, message);
}

void AlterTableDialog::alterButton_clicked()
{
	if (m_alteringActive && !(creator && creator->checkForPending()))
	{
		return;
	}

	QString newTableName(ui.nameEdit->text());
	QString oldTableName(m_item->text(0));
	if (m_dubious)
	{
		int ret = QMessageBox::question(this, "Sqliteman",
			tr("A table with an empty column name "
			   "will not display correctly.\n"
			   "Are you sure you want to go ahead?\n\n"
			   "Yes to do it anyway, Cancel to try again."),
			QMessageBox::Yes, QMessageBox::Cancel);
		if (ret == QMessageBox::Cancel) { return; }
	}
	if (newTableName.contains(QRegExp
		("\\s|-|\\]|\\[|[`!\"%&*()+={}:;@'~#|\\\\<,>.?/^]")))
	{
		int ret = QMessageBox::question(this, "Sqliteman",
			tr("A table named ")
			+ newTableName
			+ tr(" will not display correctly.\n"
				 "Are you sure you want to rename it?\n\n"
				 "Yes to rename, Cancel to try another name."),
			QMessageBox::Yes, QMessageBox::Cancel);
		if (ret == QMessageBox::Cancel) { return; }
	}
	ui.resultEdit->setHtml("");
	if (!execSql("SAVEPOINT ALTER_TABLE;", tr("Cannot create savepoint")))
	{
		return;
	}
	if (newTableName.compare(oldTableName, Qt::CaseInsensitive) && !m_altered)
	{
		// only need to rename
		if (!renameTable(oldTableName, newTableName))
		{
			doRollback(tr("Cannot roll back after error"));
			return;
		}
		if (!execSql("RELEASE ALTER_TABLE;", tr("Cannot release savepoint")))
		{
			if (doRollback(tr("Cannot roll back either")))
			{
				return;
			}
		}
		updated = true;
		m_item->setText(0, newTableName);
		return;
	}

	if (newTableName.compare(oldTableName, Qt::CaseInsensitive) == 0)
	{
		// generate unique temporary tablename
		QString tmpName = Database::getTempName(m_item->text(1));
		if (!renameTable(oldTableName, tmpName))
		{
			doRollback(tr("Cannot roll back after error"));
			return;
		}
		oldTableName = tmpName;
		if (!m_altered)
		{
			// only need to rename again (case changed)
			if (!renameTable(oldTableName, newTableName))
			{
				if (!doRollback(tr("Cannot roll back after error")))
				{
					updated = true;
					m_item->setText(0, oldTableName);
				}
				return;
			}
			if (!execSql("RELEASE ALTER_TABLE;",
						 tr("Cannot release savepoint")))
			{
				if (doRollback(tr("Cannot roll back either")))
				{
					return;
				}
			}
			updated = true;
			m_item->setText(0, newTableName);
			return;
		}
	}

	// Here newTableName differs from oldTableName in more than case,
	// and we have column changes to make
    QList<FieldInfo> oldColumns =
        Database::tableFields(oldTableName, m_item->text(1));
	// indexes and triggers on the original table
	QStringList originalSrc = originalSource(oldTableName);

	m_keptColumns.clear();
	QString message(tr("Cannot create table ") + newTableName);
	QString sql(getFullName() + " ( " + getSQLfromGUI());
	if (!execSql(sql, message))
	{
		if (!doRollback(tr("Cannot roll back after error")))
		{
			updated = true;
			m_item->setText(0, oldTableName);
		}
		return;
	}

	// insert old data
	if (m_keptColumns.count() > 0)
	{
		QString insert(QString("INSERT INTO %1.%2 (")
			.arg(Utils::quote(ui.databaseCombo->currentText()),
				 Utils::quote(ui.nameEdit->text())));

	    QString select(" ) SELECT ");
		bool first = true;
		foreach (int i, m_keptColumns)
		{
			if (first) { first = false; }
			else
			{
				insert += ", ";
				select += ", ";
			}
			QLineEdit * nameItem =
				qobject_cast<QLineEdit *>(ui.columnTable->cellWidget(i, 0));
			insert += Utils::quote(nameItem->text());
	        select += Utils::quote(oldColumns[i].name);
		}
		select += " FROM "
				  + Utils::quote(m_item->text(1))
				  + "."
				  + Utils::quote(oldTableName)
				  + ";";
		message = tr("Cannot insert data into ")
				  + newTableName;
		if (!execSql(insert + select, message))
		{
			if (!doRollback(tr("Cannot roll back after error")))
			{
				updated = true;
				m_item->setText(0, oldTableName);
			}
			return;
		}
	}
	
	// drop old table
	sql = "DROP TABLE ";
	sql += Utils::quote(m_item->text(1))
		   + "."
		   + Utils::quote(oldTableName)
		   + ";";
	message = tr("Cannot drop table ")
			  + oldTableName;
	if (!execSql(sql, message))
	{
		if (!doRollback(tr("Cannot roll back after error")))
		{
			updated = true;
			m_item->setText(0, oldTableName);
		}
		return;
	}

	// restoring original indexes and triggers
	// FIXME fix up if columns dropped or renamed
	foreach (QString restoreSql, originalSrc)
	{
		// continue after failure here
		(void)execSql(restoreSql, tr("Cannot recreate original index/trigger"));
	}

	if (!execSql("RELEASE ALTER_TABLE;", tr("Cannot release savepoint")))
	{
		if (doRollback(tr("Cannot roll back either")))
		{
			return;
		}
	}
	updated = true;
	m_item->setText(0, newTableName);
	resetStructure();
	resultAppend(tr("Alter Table Done"));
}

void AlterTableDialog::addField()
{
	int i = ui.columnTable->rowCount();
	TableEditorDialog::addField();
	QTableWidgetItem * wi = new QTableWidgetItem(0);
	wi->setFlags(Qt::NoItemFlags);
	ui.columnTable->setItem(i, 4, wi);
	wi = new QTableWidgetItem(0);
	wi->setFlags(Qt::NoItemFlags);
	ui.columnTable->setItem(i, 5, wi);
}

void AlterTableDialog::removeField()
{
	if (ui.columnTable->currentRow() < m_fields.count())
		return;
	TableEditorDialog::removeField();
}

void AlterTableDialog::fieldSelected()
{
	if (ui.columnTable->currentRow() < m_fields.count())
	{
		ui.removeButton->setEnabled(false);
		return;
	}
	TableEditorDialog::fieldSelected();
}

void AlterTableDialog::cellClicked(int row, int)
{
	if (row < m_fields.count())
	{
		ui.removeButton->setEnabled(false);
		return;
	}
	TableEditorDialog::fieldSelected();
}

bool AlterTableDialog::checkRetained(int i)
{
	if (i < m_fields.count())
	{
		QCheckBox * drop =
			qobject_cast<QCheckBox*>(ui.columnTable->cellWidget(i, 5));
		if (drop->checkState() == Qt::Checked) { return false; }
		m_keptColumns.append(i);
	}
	return true;
}

bool AlterTableDialog::checkColumn(int i, QString cname,
								   QString ctype, QString cextra)
{
	if (!checkRetained(i))
	{
		m_altered = true;
		return true;
	}
	if (i < m_fields.count())
	{
		bool useNull = m_prefs->nullHighlight();
		QString nullText = m_prefs->nullHighlightText();

		QString ftype(m_fields[i].type);
		if (ftype.isEmpty())
		{
			if (useNull && !nullText.isEmpty())
			{
				ftype = nullText;
			}
			else
			{
				ftype = "NULL";
			}
		}
		QString fextra;
		if (m_fields[i].isAutoIncrement)
		{
			if (!fextra.isEmpty()) { fextra.append(" "); }
			fextra.append("AUTOINCREMENT");
		}
		if (m_fields[i].isPartOfPrimaryKey)
		{
			if (!m_fields[i].isAutoIncrement)
			{
				if (!fextra.isEmpty()) { fextra.append(" "); }
				fextra.append("PRIMARY KEY");
			}
		}
		if (m_fields[i].isNotNull)
		{
			if (!fextra.isEmpty()) { fextra.append(" "); }
			fextra.append("NOT NULL");
		}
		QLineEdit * defval =
			qobject_cast<QLineEdit*>(ui.columnTable->cellWidget(i, 3));
		if (   (cname != m_fields[i].name)
			|| (ctype != ftype)
			|| (fextra != cextra)
			|| (defval->text() != SqlParser::defaultToken(m_fields[i])))
		{
			m_altered = true;
		}
	}
	else
	{
		m_altered = true;
	}
	return false;
}

void AlterTableDialog::checkChanges()
{
	m_dubious = false;
	m_keptColumns.clear();
	QString newName(ui.nameEdit->text());
	m_altered = m_hadRowid == ui.withoutRowid->isChecked();
	bool ok = checkOk(newName); // side-effect on m_altered
	m_alterButton->setEnabled(   ok
							  && (   m_altered
								  || (newName != m_item->text(0))));
}
