/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
	FIXME handle all columns replaced
	FIXME avoid recreating if sqlite ALTER TABLE can do it
	FIXME allow reordering columns
*/

#include <QCheckBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QTreeWidgetItem>
#include <QMessageBox>
#include <QSettings>

#include "altertabledialog.h"
#include "utils.h"
#include "preferences.h"


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
	ui.removeButton->setEnabled(false);
	setWindowTitle(tr("Alter Table"));

	if (m_item)
	{
		ui.nameEdit->setText(m_item->text(0));
		int i = ui.databaseCombo->findText(m_item->text(1),
			Qt::MatchFixedString | Qt::MatchCaseSensitive);
		if (i >= 0)
		{
			ui.databaseCombo->setCurrentIndex(i);
			ui.databaseCombo->setDisabled(true);
		}
	}
	ui.tabWidget->removeTab(2);
	ui.tabWidget->removeTab(1);
	ui.adviceLabel->hide();
	m_alterButton =
		ui.buttonBox->addButton("Alte&r", QDialogButtonBox::ApplyRole);
	m_alterButton->setDisabled(true);
	connect(m_alterButton, SIGNAL(clicked(bool)),
			this, SLOT(alterButton_clicked()));

	ui.columnTable->insertColumn(4); // show if it's indexed
	QTableWidgetItem * captIx = new QTableWidgetItem(tr("Indexed"));
	ui.columnTable->setHorizontalHeaderItem(4, captIx);
	ui.columnTable->insertColumn(5); // drop protected columns
	QTableWidgetItem * captDrop = new QTableWidgetItem(tr("Drop"));
	ui.columnTable->setHorizontalHeaderItem(5, captDrop);

	connect(ui.columnTable, SIGNAL(cellClicked(int, int)),
			this, SLOT(cellClicked(int,int)));
	connect(ui.nameEdit, SIGNAL(textEdited(const QString&)),
			this, SLOT(checkChanges()));

	resetStructure();
}

void AlterTableDialog::resetStructure()
{
	Preferences * prefs = Preferences::instance();
	bool useNull = prefs->nullHighlight();
	QString nullText = prefs->nullHighlightText();

	// obtain all indexed columns for DROP COLUMN checks
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
	m_fields = Database::tableFields(m_item->text(0), m_item->text(1));
	ui.columnTable->clearContents();
	ui.columnTable->setRowCount(m_fields.size());
	for(int i = 0; i < m_fields.size(); i++)
	{
		QLineEdit * name = new QLineEdit(this);
		name->setText(m_fields[i].name);
		name->setFrame(false);
		connect(name, SIGNAL(textEdited(const QString &)),
				this, SLOT(checkChanges()));
		ui.columnTable->setCellWidget(i, 0, name);

		QComboBox * box = new QComboBox(this);
		box->setEditable(true);
		box->setSizeAdjustPolicy(QComboBox::AdjustToContents);
		QStringList types;
		if (useNull && !nullText.isEmpty()) { types << nullText; }
		else { types << "Null"; }
		types << "Text" << "PK Integer"  << "PK Autoincrement"
			  << "Integer" << "Real" << "Blob";
		box->addItems(types);
		int n;
		QString s = m_fields[i].type;
		if (s.isEmpty())
		{
			n = 0;
		}
		else
		{
			n = box->findText(s, Qt::MatchFixedString | Qt::MatchCaseSensitive);
			if (n <= 0)
			{
				n = box->count();
				box->addItem(s);
			}
		}
		box->setCurrentIndex(n);
		connect(box, SIGNAL(currentIndexChanged(int)),
				this, SLOT(checkChanges()));
		connect(box, SIGNAL(editTextChanged(const QString &)),
				this, SLOT(checkChanges()));
		ui.columnTable->setCellWidget(i, 1, box);

		QCheckBox *nn = new QCheckBox(this);
		nn->setCheckState(m_fields[i].notnull ? Qt::Checked : Qt::Unchecked);
		connect(nn, SIGNAL(stateChanged(int)), this, SLOT(checkChanges()));
		ui.columnTable->setCellWidget(i, 2, nn);

		QLineEdit * defval = new QLineEdit(this);
		s = m_fields[i].defval;
		defval->setText(s);
		if (useNull && s.isNull())
		{
			defval->setPlaceholderText(nullText);
		}
		defval->setFrame(false);
		connect(defval, SIGNAL(textEdited(const QString &)),
				this, SLOT(checkChanges()));
		ui.columnTable->setCellWidget(i, 3, defval);

		QTableWidgetItem * ixItem = new QTableWidgetItem();
		ixItem->setFlags(Qt::ItemIsSelectable);
		if (m_columnIndexMap.contains(m_fields[i].name))
		{
			ixItem->setIcon(Utils::getIcon("index.png"));
			ixItem->setText(tr("Yes"));
		}
		else { ixItem->setText(tr("No")); }
		ui.columnTable->setItem(i, 4, ixItem);

		QCheckBox * dropItem = new QCheckBox(this);
		connect(dropItem, SIGNAL(stateChanged(int)),
				this, SLOT(dropItem_stateChanged(int)));
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

bool AlterTableDialog::execSql(const QString & statement, const QString & message)
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

void AlterTableDialog::doRollback(QString message)
{
	if (execSql("ROLLBACK TO ALTER_TABLE;", message))
	{
		// rollback does not cancel the savepoint
		if (execSql("RELEASE ALTER_TABLE;", QString("")))
		{
			return;
		}
	}
	resultAppend(tr("Database may be left with a pending savepoint."));
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
						  + ":<br/><span style=\" color:#ff0000;\">"
						  + query.lastError().text()
						  + "<br/></span>" + tr("using sql statement:")
						  + "<br/><tt>" + ixsql;
		resultAppend(errtext);
		return QStringList();
	}
	while(query.next())
		ret.append(query.value(0).toString());
	return ret;
}

bool AlterTableDialog::renameTable(QString newTableName)
{
	QString oldName = m_item->text(0);
	if (oldName.compare(newTableName, Qt::CaseInsensitive) == 0)
	{
		if (oldName.compare(newTableName, Qt::CaseSensitive) == 0)
		{
			return true;
		}
		else
		{
			// sqlite won't rename if only case changes,
			// so we rename via a temporary name
			QString tmpName = Database::getTempName(m_item->text(1));
			QString sql = QString("ALTER TABLE ")
						  + Utils::quote(m_item->text(1))
						  + "."
						  + Utils::quote(oldName)
						  + " RENAME TO "
						  + Utils::quote(tmpName)
						  + ";";
			QString message = tr("Cannot rename table ")
							  + oldName
							  + tr(" to ")
							  + tmpName;
			if (!execSql(sql, message))
			{
				return false;
			}
			oldName = tmpName;
		}
	}
	QString sql = QString("ALTER TABLE ")
				  + Utils::quote(m_item->text(1))
				  + "."
				  + Utils::quote(oldName)
				  + " RENAME TO "
				  + Utils::quote(newTableName)
				  + ";";
	QString message = tr("Cannot rename table ")
					  + oldName
					  + tr(" to ")
					  + newTableName;
	if (execSql(sql, message))
	{
		updateStage = 1;
		m_item->setText(0, newTableName);
		return true;
	}
	return false;
}

void AlterTableDialog::alterButton_clicked()
{
	ui.resultEdit->setHtml("");
	if (m_alteringActive && !(creator && creator->checkForPending()))
	{
		return;
	}
	// rename table if it's required
	QString newTableName(ui.nameEdit->text().trimmed());
	if (newTableName.contains(QRegExp
		("\\s|-|\\]|\\[|[`!\"%&*()+={}:;@'~#|\\\\<,>.?/^]")))
	{
		int ret = QMessageBox::question(this, "Sqliteman",
			tr("A table named ")
			+ newTableName
			+ tr(" will not display correctly. "
				 "Are you sure you want to rename it?\n")
			+ tr("\nYes to rename, Cancel to try another name."),
			QMessageBox::Yes, QMessageBox::Cancel);
		if (ret == QMessageBox::Cancel) { return; }
	}
	if (!execSql("SAVEPOINT ALTER_TABLE;", tr("Cannot create savepoint")))
	{
		return;
	}
	if (!renameTable(newTableName)) {
		doRollback(tr("Cannot roll back after error"));
		return;
	}

	// drop columns first
// 	if (m_dropColumns > 0)
	{
        FieldList oldColumns =
	        Database::tableFields(m_item->text(0), m_item->text(1));
		// indexes and triggers on the original table
		QStringList originalSrc = originalSource();

		// generate unique temporary tablename
		QString tmpName = Database::getTempName(m_item->text(1));

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
			doRollback(tr("Cannot roll back after error"));
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
			doRollback(tr("Cannot roll back after error"));
			return;
		}

		// insert old data
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
		{
			doRollback(tr("Cannot roll back after error"));
			return;
		}
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
		{
			doRollback(tr("Cannot roll back after error"));
			return;
		}

		// restoring original indexes
		foreach (QString restoreSql, originalSrc)
		{
			if (!execSql(restoreSql,
				tr("Cannot recreate original index/trigger")))
			{
				doRollback(tr("Cannot roll back after error"));
				return;
			}
		}
	}

	// handle add columns
	if (!addColumns())
	{
		doRollback(tr("Cannot roll back after error"));
		return;
	}

	if (!execSql("RELEASE ALTER_TABLE;", tr("Cannot release savepoint")))
	{
		doRollback(tr("Cannot roll back either"));
		return;
	}
	if (updateStage > 0)
	{
		resetStructure();
		resultAppend(tr("Alter Table Done"));
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
							  + ":<br/><span style=\" color:#ff0000;\">"
							  + query.lastError().text()
							  + "<br/></span>" + tr("using sql statement:")
							  + "<br/><tt>" + fullSql;
			resultAppend(errtext);
			return false;
		}
		updateStage = 2;
	}
	resultAppend(tr("Columns added successfully"));
	return true;
}

void AlterTableDialog::removeField()
{
	if (ui.columnTable->currentRow() < m_protectedRows)
		return;
	TableEditorDialog::removeField();
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
	Preferences * prefs = Preferences::instance();
	bool useNull = prefs->nullHighlight();
	QString nullText = prefs->nullHighlightText();

	QString newName = ui.nameEdit->text().trimmed();
	int cols = ui.columnTable->rowCount();
	int colsLeft = cols;
	bool changed =   (m_dropColumns != 0)
				  || (m_item->text(0).compare(newName, Qt::CaseSensitive))
				  || (m_protectedRows != cols);

	bool bad = newName.isEmpty();
	
	for (int i = 0; i < cols; i++)
	{
		QLineEdit * name =
			qobject_cast<QLineEdit*>(ui.columnTable->cellWidget(i, 0));
		if (name->text().isEmpty())
		{
			bad = true;
			break;
		}
		if (i < m_fields.count())
		{
			QComboBox * type =
				qobject_cast<QComboBox*>(ui.columnTable->cellWidget(i, 1));
			QString ftype = m_fields[i].type;
			if (ftype.isEmpty())
			{
				if (useNull && !nullText.isEmpty())
				{
					ftype = nullText;
				}
				else
				{
					ftype = "Null";
				}
			}
			QString ctype = type->currentText();
			QCheckBox * nn =
				qobject_cast<QCheckBox*>(ui.columnTable->cellWidget(i, 2));
			QLineEdit * defval =
				qobject_cast<QLineEdit*>(ui.columnTable->cellWidget(i, 3));
			QCheckBox * drop =
				qobject_cast<QCheckBox*>(ui.columnTable->cellWidget(i, 5));
			if (   (name->text() != m_fields[i].name)
				|| (ftype != ctype)
				|| (defval->text() != m_fields[i].defval)
				|| (nn->checkState() !=
					(m_fields[i].notnull ? Qt::Checked : Qt::Unchecked)))
			{
				changed = true;
			}
			if (drop->checkState() == Qt::Checked) { --colsLeft; }
		}
	}
	if (colsLeft == 0) { bad = true; }
	m_alterButton->setEnabled(changed && !bad);
}
