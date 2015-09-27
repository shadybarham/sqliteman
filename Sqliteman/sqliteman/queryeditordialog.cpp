/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QScrollBar>
#include <QComboBox>
#include <QLineEdit>
#include <QSettings>
#include <QTreeWidgetItem>

#include "queryeditordialog.h"
#include "utils.h"

QueryStringModel::QueryStringModel(QObject * parent)
	: QStringListModel(parent)
{
}

Qt::ItemFlags QueryStringModel::flags (const QModelIndex & index) const
{
	return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

void QueryStringModel::clear()
{
	setStringList(QStringList());
}

void QueryStringModel::append(const QString & value)
{
	QStringList l(stringList());
	l.append(value);
	setStringList(l);
}

void QueryEditorDialog::CommonSetup()
{
	setupUi(this);
	QSettings settings("yarpen.cz", "sqliteman");
	int hh = settings.value("queryeditor/height", QVariant(500)).toInt();
	int ww = settings.value("queryeditor/width", QVariant(600)).toInt();
	resize(ww, hh);

	columnModel = new QueryStringModel(this);
	selectModel = new QueryStringModel(this);
	columnView->setModel(columnModel);
	selectView->setModel(selectModel);
	
	termsTable->setColumnCount(3);
	termsTable->horizontalHeader()->hide();
	termsTable->verticalHeader()->hide();
	termsTable->setShowGrid(false);
	ordersTable->setColumnCount(3);
	ordersTable->horizontalHeader()->hide();
	ordersTable->verticalHeader()->hide();
	ordersTable->setShowGrid(false);

	connect(tableList, SIGNAL(activated(const QString &)),
			this, SLOT(tableSelected(const QString &)));
	connect(termMoreButton, SIGNAL(clicked()), this, SLOT(moreTerms()));
	connect(termLessButton, SIGNAL(clicked()), this, SLOT(lessTerms()));
	//
	connect(addAllButton, SIGNAL(clicked()), this, SLOT(addAllSelect()));
	connect(addButton, SIGNAL(clicked()), this, SLOT(addSelect()));
	connect(removeAllButton, SIGNAL(clicked()), this, SLOT(removeAllSelect()));
	connect(removeButton, SIGNAL(clicked()), this, SLOT(removeSelect()));
	connect(columnView, SIGNAL(doubleClicked(const QModelIndex &)),
			this, SLOT(addSelect()));
	connect(selectView, SIGNAL(doubleClicked(const QModelIndex &)),
			this, SLOT(removeSelect()));
	connect(orderMoreButton, SIGNAL(clicked()), this, SLOT(moreOrders()));
	connect(orderLessButton, SIGNAL(clicked()), this, SLOT(lessOrders()));
}


QueryEditorDialog::QueryEditorDialog(QWidget * parent): QDialog(parent)
{
	CommonSetup();

	m_schema = "main";

	QStringList tables = Database::getObjects("table").keys();
	tableList->addItems(tables);
	// FIXME need to fix create parser before this will work
//	tables = Database::getObjects("view").keys();
//	tableList->addItems(tables);
	
	// If a database has at least one table. auto select it
	if(tables.size() > 0)
		tableSelected(tables[0]); 
}

QueryEditorDialog::QueryEditorDialog(QTreeWidgetItem * item, QWidget * parent)
	: QDialog(parent)
{
	CommonSetup();

	m_schema = item->text(1);

	tableList->addItem(item->text(0));
	tableList->setEnabled(false);
	tableSelected(item->text(0)); 
}

QueryEditorDialog::~QueryEditorDialog()
{
	QSettings settings("yarpen.cz", "sqliteman");
    settings.setValue("queryeditor/height", QVariant(height()));
    settings.setValue("queryeditor/width", QVariant(width()));
}

QString QueryEditorDialog::statement()
{
	QString logicWord;
	QString sql = "SELECT ";

	// columns
	if (selectModel->rowCount() == 0)
		sql += "* ";
	else
	{
		sql += Utils::quote(selectModel->stringList());
	}

	// Add table name
			sql += (" FROM " + Utils::quote(m_schema) + "." +
					Utils::quote(tableList->currentText()));

	// Optionaly add terms
	if (termsTable->rowCount() > 0)
	{
		// But first determine what is the chosen logic word (And/Or)
		(andButton->isChecked()) ? logicWord = " AND " : logicWord = " OR ";

		sql += " WHERE ";

		for(int i = 0; i < termsTable->rowCount(); i++)
		{
			QComboBox * fields =
				qobject_cast<QComboBox *>(termsTable->cellWidget(i, 0));
			QComboBox * relations =
				qobject_cast<QComboBox *>(termsTable->cellWidget(i, 1));
			connect(relations,
					SIGNAL(currentIndexChanged(const QString & text)),
					this, SLOT(relationChanged(const QString & text)));
			QLineEdit * value =
				qobject_cast<QLineEdit *>(termsTable->cellWidget(i, 2));
			if (fields && relations && value)
			{
				if (i > 0) { sql += logicWord; }
				sql += Utils::quote(fields->currentText());

				switch (relations->currentIndex())
				{
					case 0:	// Contains
						sql += (" LIKE " + Utils::like(value->text()));
						break;

					case 1: 	// Doesn't contain
						sql += (" NOT LIKE "
								+ Utils::like(value->text()));
						break;

					case 2:		// Equals
						sql += (" = " + Utils::literal(value->text()));
						break;

					case 3:		// Not equals
						sql += (" <> " + Utils::literal(value->text()));
						break;

					case 4:		// Bigger than
						sql += (" > " + Utils::literal(value->text()));
						break;

					case 5:		// Smaller than
						sql += (" < " + Utils::literal(value->text()));
						break;

					case 6:		// is null
						sql += (" ISNULL");
						break;

					case 7:		// is not null
						sql += (" NOTNULL");
						break;
				}
			}
		}
	}

	// optionally add ORDER BY clauses
	if (ordersTable->rowCount() > 0)
	{
		sql += " ORDER BY ";
		for (int i = 0; i < ordersTable->rowCount(); i++)
		{
			QComboBox * fields =
				qobject_cast<QComboBox *>(ordersTable->cellWidget(i, 0));
			QComboBox * collators =
				qobject_cast<QComboBox *>(ordersTable->cellWidget(i, 1));
			QComboBox * directions =
				qobject_cast<QComboBox *>(ordersTable->cellWidget(i, 2));
			if (fields && collators && directions)
			{
				if (i > 0) { sql += ", "; }
				sql += Utils::quote(fields->currentText()) + " COLLATE ";
				sql += collators->currentText() + " ";
				sql += directions->currentText();
			}
		}
	}

	sql += ";";
	return sql;
}

void QueryEditorDialog::tableSelected(const QString & table)
{
	FieldList fields = Database::tableFields(table, m_schema);
	curTable = table;
	m_columnList.clear();

	bool rowid = true;
	bool _rowid_ = true;
	bool oid = true;
	foreach(DatabaseTableField i, fields)
	{
		if (i.name.compare("rowid", Qt::CaseInsensitive) == 0)
			{ rowid = false; }
		if (i.name.compare("_rowid_", Qt::CaseInsensitive) == 0)
			{ _rowid_ = false; }
		if (i.name.compare("oid", Qt::CaseInsensitive) == 0)
			{ oid = false; }
	}
	if (rowid)
		{ m_columnList << QString("rowid"); m_rowid = "rowid"; }
	else if (_rowid_)
		{ m_columnList << QString("_rowid_"); m_rowid = "_rowid_"; }
	else if (oid)
		{ m_columnList << QString("oid"); m_rowid = "oid"; }

	foreach(DatabaseTableField i, fields)
		m_columnList << i.name;

	columnModel->setStringList(m_columnList);
	selectModel->clear();
	tabWidget->setCurrentIndex(0);

	// clear terms and orders
	termsTable->clear();
	termsTable->setRowCount(0); // clear() doesn't seem to do this
	ordersTable->clear();
	ordersTable->setRowCount(0); // clear() doesn't seem to do this
}


void QueryEditorDialog::addAllSelect()
{
	QStringList list(columnModel->stringList());
	foreach (QString s, list)
	{
		if (s.compare(m_rowid))
		{
			selectModel->append(s);
			list.removeAll(s);
		}
	}
	columnModel->setStringList(list);
}

void QueryEditorDialog::addSelect()
{
	QItemSelectionModel *selections = columnView->selectionModel();
	if (!selections->hasSelection())
		return;
	QStringList list(columnModel->stringList());
	QString val;
	foreach (QModelIndex i, selections->selectedIndexes())
	{
		val = columnModel->data(i, Qt::DisplayRole).toString();
		selectModel->append(val);
		list.removeAll(val);
	}
	columnModel->setStringList(list);
}

void QueryEditorDialog::removeAllSelect()
{
	tableSelected(curTable);
	selectModel->clear();
}

void QueryEditorDialog::removeSelect()
{
	QItemSelectionModel *selections = selectView->selectionModel();
	if (!selections->hasSelection())
		return;
	QStringList list(selectModel->stringList());
	QString val;
	foreach (QModelIndex i, selections->selectedIndexes())
	{
		val = selectModel->data(i, Qt::DisplayRole).toString();
		columnModel->append(val);
		list.removeAll(val);
	}
	selectModel->setStringList(list);
	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(selectModel->rowCount()!=0);
}

void QueryEditorDialog::moreTerms()
{
	int i = termsTable->rowCount();
	termsTable->setRowCount(i + 1);
	QComboBox * fields = new QComboBox();
	fields->addItems(m_columnList);
	termsTable->setCellWidget(i, 0, fields);
	QComboBox * relations = new QComboBox();
	relations->addItems(QStringList() << tr("Contains") << tr("Doesn't contain")
									  << tr("Equals") << tr("Not equals")
									  << tr("Bigger than") << tr("Smaller than")
									  << tr("is null") << tr("is not null"));
	termsTable->setCellWidget(i, 1, relations);
	connect(relations, SIGNAL(currentIndexChanged(const QString &)),
			this, SLOT(relationsIndexChanged(const QString &)));
	QLineEdit * value = new QLineEdit();
	termsTable->setCellWidget(i, 2, value);
	termsTable->resizeColumnsToContents();
	termLessButton->setEnabled(true);
}

void QueryEditorDialog::lessTerms()
{
	int i = termsTable->rowCount() - 1;
	termsTable->removeRow(i);
	termsTable->resizeColumnsToContents();
	if (i == 0)
		termLessButton->setEnabled(false);
}

void QueryEditorDialog::moreOrders()
{
	int i = ordersTable->rowCount();
	ordersTable->setRowCount(i + 1);
	QComboBox * fields = new QComboBox();
	fields->addItems(m_columnList);
	ordersTable->setCellWidget(i, 0, fields);
	QComboBox * collators = new QComboBox();
	collators->addItem("BINARY");
	collators->addItem("NOCASE");
	collators->addItem("RTRIM");
	ordersTable->setCellWidget(i, 1, collators);
	QComboBox * directions = new QComboBox();
	directions->addItem("ASC");
	directions->addItem("DESC");
	ordersTable->setCellWidget(i, 2, directions);
	ordersTable->resizeColumnsToContents();
	orderLessButton->setEnabled(true);
}

void QueryEditorDialog::lessOrders()
{
	int i = ordersTable->rowCount() - 1;
	ordersTable->removeRow(i);
	if (i == 0)
		orderLessButton->setEnabled(false);
}

void QueryEditorDialog::relationsIndexChanged(const QString &)
{
	QComboBox * relations = qobject_cast<QComboBox *>(sender());
	for (int i = 0; i < termsTable->rowCount(); ++i)
	{
		if (relations == termsTable->cellWidget(i, 1))
		{
			QLineEdit * value =
				qobject_cast<QLineEdit *>(termsTable->cellWidget(i, 2));
			if (value)
			{
				switch (relations->currentIndex())
				{
					case 0: value->show(); // Contains
						return;

					case 1: value->show(); // Doesn't contain
						return;

					case 2: value->show(); // Equals
						return;

					case 3: value->show(); // Not equals
						return;

					case 4: value->show(); // Bigger than
						return;

					case 5: value->show(); // Smaller than
						return;

					case 6: value->hide(); // is null
						return;

					case 7: value->hide(); // is not null
						return;
				}
			}
		}
	}
}
