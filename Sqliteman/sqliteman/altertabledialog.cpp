/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
	FIXME icon-only widths, not removing column, up/down not altering
	FIXME loses some constraints
	multiple primary key are not allowed
	multiple not null are allowed
	multiple unique are allowed if any on conflict clauses are the same
	multiple different checks are allowed
	multiple default are possible, last one is used
	multiple collate are possible even with different collation names
*/

#include <QtCore/qmath.h>
#include <QCheckBox>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QTreeWidgetItem>

#include "altertabledialog.h"
#include "litemanwindow.h"
#include "mylineedit.h"
#include "sqlparser.h"
#include "utils.h"


void AlterTableDialog::addField(QString oldName, QString oldType,
								int x, QString oldDefault)
{
	int i = ui.columnTable->rowCount();
	TableEditorDialog::addField(oldName, oldType, x, oldDefault);
	if (i > 0)
	{
		ui.columnTable->item(i - 1, 5)->setFlags(Qt::ItemIsEnabled);
	}
	QTableWidgetItem * it = new QTableWidgetItem();
	it->setIcon(Utils::getIcon("move-up.png"));
	it->setToolTip("move up");
	it->setFlags((i == 0) ? Qt::NoItemFlags : Qt::ItemIsEnabled);
	ui.columnTable->setItem(i, 4, it);
	it = new QTableWidgetItem();
	it->setIcon(Utils::getIcon("move-down.png"));
	it->setToolTip("move down");
	it->setFlags(Qt::NoItemFlags);
	ui.columnTable->setItem(i, 5, it);
	it =new QTableWidgetItem();
	it->setIcon(Utils::getIcon("delete.png"));
	it->setToolTip("remove");
	it->setFlags(Qt::ItemIsEnabled);
	ui.columnTable->setItem(i, 6, it);
	m_isIndexed.append(false);
	m_oldColumn.append(-1);
}

void AlterTableDialog::addField()
{
	addField(QString(), QString(), 0, QString());
}

void AlterTableDialog::resetClicked()
{
	// Initialize fields
	SqlParser * parsed = Database::parseTable(m_item->text(0), m_item->text(1));
	m_fields = parsed->m_fields;
	ui.columnTable->clearContents();
	ui.columnTable->setRowCount(0);
	int n = m_fields.size();
	m_isIndexed.fill(false, n);
	m_oldColumn.resize(n);
	int i;
	for (i = 0; i < n; i++)
	{
		m_oldColumn[i] = i;
		QString name(m_fields[i].name);
		int x = m_fields[i].isAutoIncrement ? 3
				: ( m_fields[i].isPartOfPrimaryKey ? 2
					: (m_fields[i].isNotNull ? 1 : 0));

		addField(name, m_fields[i].type, x,
				 SqlParser::defaultToken(m_fields[i]));
	}

	// obtain all indexed columns for DROP COLUMN checks
	QMap<QString,QStringList> columnIndexMap;
	foreach(QString index,
		Database::getObjects("index", m_item->text(1)).values(m_item->text(0)))
	{
		foreach(QString indexColumn,
			Database::indexFields(index, m_item->text(1)))
		{
			for (i = 0; i < n; i++)
			{
				if (!(m_fields[i].name.compare(
					indexColumn, Qt::CaseInsensitive)))
				{
					m_isIndexed[i] = true;
				}
			}
		}
	}

	m_hadRowid = parsed->m_hasRowid;
	ui.withoutRowid->setChecked(!m_hadRowid);
	delete parsed;
	m_dropped = false;
	checkChanges();
}

AlterTableDialog::AlterTableDialog(LiteManWindow * parent,
								   QTreeWidgetItem * item,
								   const bool isActive)
	: TableEditorDialog(parent),
	m_item(item)
{
	m_alteringActive = isActive;
	ui.removeButton->setEnabled(false);
	ui.removeButton->hide();
	setWindowTitle(tr("Alter Table"));
	m_tableOrView = "TABLE";
	m_dubious = false;
	m_oldWidget = ui.designTab;

	m_alterButton =
		ui.buttonBox->addButton("Alte&r", QDialogButtonBox::ApplyRole);
	m_alterButton->setDisabled(true);
	connect(m_alterButton, SIGNAL(clicked(bool)),
			this, SLOT(alterButton_clicked()));
	QPushButton * resetButton = new QPushButton("Reset", this);
	connect(resetButton, SIGNAL(clicked(bool)), this, SLOT(resetClicked()));
	ui.tabWidget->setCornerWidget(resetButton, Qt::TopRightCorner);

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

	ui.columnTable->insertColumn(4);
	ui.columnTable->setHorizontalHeaderItem(4, new QTableWidgetItem());
	ui.columnTable->insertColumn(5);
	ui.columnTable->setHorizontalHeaderItem(5, new QTableWidgetItem());
	ui.columnTable->insertColumn(6);
	ui.columnTable->setHorizontalHeaderItem(6, new QTableWidgetItem());

	connect(ui.columnTable, SIGNAL(cellClicked(int, int)),
			this, SLOT(cellClicked(int,int)));

	resetClicked();
}

bool AlterTableDialog::execSql(const QString & statement,
							   const QString & message)
{
	QSqlQuery query(statement, QSqlDatabase::database(SESSION_NAME));
	if (query.lastError().isValid())
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
					+ " where type = 'trigger' and tbl_name = "
					+ Utils::quote(tableName)
					+ ";";
	QSqlQuery query(ixsql, QSqlDatabase::database(SESSION_NAME));

	if (query.lastError().isValid())
	{
		QString errtext = tr("Cannot get trigger list for ")
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

QList<SqlParser *> AlterTableDialog::originalIndexes(QString tableName)
{
	QList<SqlParser *> ret;
	QString ixsql = QString("select sql from ")
					+ Database::getMaster(m_item->text(1))
					+ " where type = 'index' and tbl_name = "
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
	}
	else
	{
		while (query.next())
		{
			ret.append(new SqlParser(query.value(0).toString()));
		}
	}
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

bool AlterTableDialog::checkColumn(int i, QString cname,
								   QString ctype, QString cextra)
{
	int j = m_oldColumn[i];
	if (j >= 0)
	{
		bool useNull = m_prefs->nullHighlight();
		QString nullText = m_prefs->nullHighlightText();

		QString ftype(m_fields[j].type);
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
		if (m_fields[j].isAutoIncrement)
		{
			if (!fextra.isEmpty()) { fextra.append(" "); }
			fextra.append("AUTOINCREMENT");
		}
		if (m_fields[j].isPartOfPrimaryKey)
		{
			if (!m_fields[j].isAutoIncrement)
			{
				if (!fextra.isEmpty()) { fextra.append(" "); }
				fextra.append("PRIMARY KEY");
			}
		}
		if (m_fields[j].isNotNull)
		{
			if (!fextra.isEmpty()) { fextra.append(" "); }
			fextra.append("NOT NULL");
		}
		QLineEdit * defval =
			qobject_cast<QLineEdit*>(ui.columnTable->cellWidget(i, 3));
		if (   (j != i)
			|| (cname != m_fields[j].name)
			|| (ctype != ftype)
			|| (fextra != cextra)
			|| (defval->text() != SqlParser::defaultToken(m_fields[j])))
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

// special version for column with only icons
void AlterTableDialog::resizeTable()
{
	QTableWidget * tv = ui.columnTable;
	int widthView = tv->viewport()->width();
	int widthLeft = widthView;
	int widthUsed = 0;
	int columns = tv->horizontalHeader()->count();
	int columnsLeft = columns;
	QVector<int> wantedWidths(columns);
	QVector<int> gotWidths(columns);
	tv->resizeColumnsToContents();
	int i;
	for (i = 0; i < columns; ++i)
	{
		if (i < columns - 3)
		{
			wantedWidths[i] = tv->columnWidth(i);
		}
		else
		{
			wantedWidths[i] = 28;
		}
		gotWidths[i] = 0;
	}
	i = 0;
	/* First give all "small" columns what they want. */
	while (i < columns)
	{
		int w = wantedWidths[i];
		if ((gotWidths[i] == 0) && (w <= widthLeft / columnsLeft ))
		{
			gotWidths[i] = w;
			widthLeft -= w;
			widthUsed += w;
			columnsLeft -= 1;
			i = 0;
			continue;
		}
		++i;
	}
	/* Now allocate to other columns, giveing smaller ones a larger proportion
	 * of what they want;
	 */
	for (i = 0; i < columns; ++i)
	{
		if (gotWidths[i] == 0)
		{
			int w = (int)qSqrt((qreal)(
				wantedWidths[i] * widthLeft / columnsLeft));
			gotWidths[i] = w;
			widthUsed += w;
		}
	}
	/* If there is space left, make all columns proportionately wider to fill
	 * it.
	 */
	if (widthUsed < widthView)
	{
		for (i = 0; i < columns; ++i)
		{
			tv->setColumnWidth(i, gotWidths[i] * widthView / widthUsed);
		}
	}
	else
	{
		for (i = 0; i < columns; ++i)
		{
			tv->setColumnWidth(i, gotWidths[i]);
		}
	}
}

void AlterTableDialog::swap(int i, int j)
{
	// Much of this unpleasantness is caused by the lack of a
	// QTableWidget::takeCellWidget() function.
	MyLineEdit * iEd =
		qobject_cast<MyLineEdit *>(ui.columnTable->cellWidget(i, 0));
	MyLineEdit * jEd =
		qobject_cast<MyLineEdit *>(ui.columnTable->cellWidget(j, 0));
	QString name(iEd->text());
	iEd->setText(jEd->text());
	jEd->setText(name);
	QComboBox * iBox =
		qobject_cast<QComboBox *>(ui.columnTable->cellWidget(i, 1));
	QComboBox * jBox =
		qobject_cast<QComboBox *>(ui.columnTable->cellWidget(j, 1));
	QComboBox * iNewBox = new QComboBox(this);
	QComboBox * jNewBox = new QComboBox(this);
	int k;
	for (k = 0; k < iBox->count(); ++k)
	{
		jNewBox->addItem(iBox->itemText(k));
	}
	for (k = 0; k < jBox->count(); ++k)
	{
		iNewBox->addItem(jBox->itemText(k));
	}
	jNewBox->setCurrentIndex(iBox->currentIndex());
	iNewBox->setCurrentIndex(jBox->currentIndex());
	jNewBox->setEditable(true);
	iNewBox->setEditable(true);
	connect(jNewBox, SIGNAL(currentIndexChanged(int)),
			this, SLOT(checkChanges()));
	connect(iNewBox, SIGNAL(currentIndexChanged(int)),
			this, SLOT(checkChanges()));
	connect(jNewBox, SIGNAL(editTextChanged(const QString &)),
			this, SLOT(checkChanges()));
	connect(iNewBox, SIGNAL(editTextChanged(const QString &)),
			this, SLOT(checkChanges()));
	ui.columnTable->setCellWidget(i, 1, iNewBox); // destroys old one
	ui.columnTable->setCellWidget(j, 1, jNewBox); // destroys old one
	iBox = qobject_cast<QComboBox *>(ui.columnTable->cellWidget(i, 2));
	jBox =
		qobject_cast<QComboBox *>(ui.columnTable->cellWidget(j, 2));
	iNewBox = new QComboBox(this);
	jNewBox = new QComboBox(this);
	for (k = 0; k < iBox->count(); ++k)
	{
		jNewBox->addItem(iBox->itemText(k));
	}
	for (k = 0; k < jBox->count(); ++k)
	{
		iNewBox->addItem(jBox->itemText(k));
	}
	jNewBox->setCurrentIndex(iBox->currentIndex());
	iNewBox->setCurrentIndex(jBox->currentIndex());
	jNewBox->setEditable(false);
	iNewBox->setEditable(false);
	connect(jNewBox, SIGNAL(currentIndexChanged(int)),
			this, SLOT(checkChanges()));
	connect(iNewBox, SIGNAL(currentIndexChanged(int)),
			this, SLOT(checkChanges()));
	ui.columnTable->setCellWidget(i, 2, iNewBox); // destroys old one
	ui.columnTable->setCellWidget(j, 2, jNewBox); // destroys old one
	iEd = qobject_cast<MyLineEdit *>(ui.columnTable->cellWidget(i, 3));
	jEd = qobject_cast<MyLineEdit *>(ui.columnTable->cellWidget(j, 3));
	name = iEd->text();
	iEd->setText(jEd->text());
	jEd->setText(name);
	k = m_oldColumn[i];
	m_oldColumn[i] = m_oldColumn[j];
	m_oldColumn[j] = k;
	checkChanges();
}

void AlterTableDialog::drop(int row)
{
	if (m_isIndexed[row])
	{
		MyLineEdit * edit = qobject_cast<MyLineEdit*>(
			ui.columnTable->cellWidget(row, 0));
		int ret = QMessageBox::question(this, "Sqliteman",
			tr("Column ") + edit->text()
			+ tr(" is indexed:\ndropping it will delete the index\n"
				 "Are you sure you want to drop it?\n\n"
				 "Yes to drop it anyway, Cancel to keep it."),
			QMessageBox::Yes, QMessageBox::Cancel);
		if (ret == QMessageBox::Cancel)
		{
			return;
		}
	}
	m_isIndexed.remove(row);
	m_oldColumn.remove(row);
	ui.columnTable->setCurrentCell(row, 0);
	removeField();
	ui.columnTable->item(0, 4)->setFlags(0);
	ui.columnTable->item(ui.columnTable->rowCount() - 1, 5)->setFlags(0);
	m_dropped = true;
	checkChanges();
}

void AlterTableDialog::cellClicked(int row, int column)
{
	int n = ui.columnTable->rowCount();
	switch (column)
	{
		case 4: // move up
			if (row > 0) { swap(row - 1, row);  }
			break;
		case 5: // move down
			if (row < n - 1) { swap(row, row + 1); }
			break;
		case 6: // delete
			drop(row);
			break;
		default: return; // cell handles its editing
	}
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
			   "Yes to do it anyway, Cancel to try another name."),
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
	// and we have column changes to make.
	// SAve indexes and triggers on the original table
	QStringList originalSrc = originalSource(oldTableName);
	QList<SqlParser *> originalIx(originalIndexes(oldTableName));

	// Create the new table
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
	QMap<QString,QString> columnMap; // old => new
	QString insert;
	QString select;
	bool first = true;
	for (int i = 0; i < ui.columnTable->rowCount(); ++i)
	{
		int j = m_oldColumn[i];
		if (j >= 0)
		{
			if (first)
			{
				first = false;
				insert += (QString("INSERT INTO %1.%2 (")
					.arg(Utils::quote(ui.databaseCombo->currentText()),
						 Utils::quote(ui.nameEdit->text())));

		    select += " ) SELECT ";
			}
			else
			{
				insert += ", ";
				select += ", ";
			}
			QLineEdit * nameItem =
				qobject_cast<QLineEdit *>(ui.columnTable->cellWidget(i, 0));
			insert += Utils::quote(nameItem->text());
	        select += Utils::quote(m_fields[j].name);
			columnMap.insert(m_fields[j].name, nameItem->text());
		}
	}
	if (!insert.isEmpty())
	{
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

	// restoring original triggers
	// FIXME fix up if columns dropped or renamed
	foreach (QString restoreSql, originalSrc)
	{
		// continue after failure here
		(void)execSql(restoreSql, tr("Cannot recreate original trigger"));
	}

	// recreate indices
	while (!originalIx.isEmpty())
	{
		SqlParser * parser = originalIx.takeFirst();
		if (parser->replace(columnMap, ui.nameEdit->text()))
		{
			// continue after failure here
			(void)execSql(parser->toString(),
						  tr("Cannot recreate index ") + parser->m_indexName);
		}
		delete parser;
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
	resetClicked();
	resultAppend(tr("Alter Table Done"));
}

void AlterTableDialog::checkChanges()
{
	m_dubious = false;
	QString newName(ui.nameEdit->text());
	m_altered = m_hadRowid == ui.withoutRowid->isChecked();
	m_altered |= m_dropped;
	bool ok = checkOk(newName); // side-effect on m_altered
	m_alterButton->setEnabled(   ok
							  && (   m_altered
								  || (newName != m_item->text(0))));
}
