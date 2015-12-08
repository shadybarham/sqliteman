/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QPushButton>
#include <QComboBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QSettings>

#include "createindexdialog.h"
#include "database.h"
#include "utils.h"


CreateIndexDialog::CreateIndexDialog(const QString & tabName,
									 const QString & schema,
									 LiteManWindow * parent)
	: QDialog(parent), m_schema(schema), m_table(tabName)
{
	creator = parent;
	update = false;
	ui.setupUi(this);
	QString s("Table Name: ");
	ui.label->setText(s + m_schema + "." + m_table);

	QList<FieldInfo> columns = Database::tableFields(m_table, m_schema);
	ui.tableColumns->setRowCount(columns.size());
	ui.createButton->setDisabled(true);

	for(int i = 0; i < columns.size(); i++)
	{
		QTableWidgetItem * nameItem = new QTableWidgetItem(columns[i].name);
		nameItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		QTableWidgetItem * useItem = new QTableWidgetItem(QString::null);
		useItem->setCheckState(Qt::Unchecked);

		ui.tableColumns->setItem(i, 0, nameItem);
		if (columns[i].isPartOfPrimaryKey)
			nameItem->setIcon(Utils::getIcon("key.png"));

		ui.tableColumns->setItem(i, 1, useItem);
		QComboBox *asc = new QComboBox(this);
		asc->addItems(QStringList() << "ASC" << "DESC");
		asc->setEnabled(false);
		ui.tableColumns->setCellWidget(i, 2, asc);
	}
	QSettings settings("yarpen.cz", "sqliteman");
	int hh = settings.value("createindex/height", QVariant(500)).toInt();
	resize(width(), hh);

	connect(ui.tableColumns, SIGNAL(itemChanged(QTableWidgetItem*)),
			this, SLOT(tableColumns_itemChanged(QTableWidgetItem*)));
	connect(ui.indexNameEdit, SIGNAL(textChanged(const QString&)),
			this, SLOT(indexNameEdit_textChanged(const QString&)));
	connect(ui.createButton, SIGNAL(clicked()), this, SLOT(createButton_clicked()));
	resizeWanted = true;
}

CreateIndexDialog::~CreateIndexDialog()
{
	QSettings settings("yarpen.cz", "sqliteman");
    settings.setValue("createindex/height", QVariant(height()));
}

void CreateIndexDialog::createButton_clicked()
{
	ui.resultEdit->setHtml("");
	if (creator && creator->checkForPending())
	{
		QStringList cols;
		for (int i = 0; i < ui.tableColumns->rowCount(); ++i)
		{
			if (ui.tableColumns->item(i, 1)->checkState() == Qt::Checked)
			{
				QComboBox* cb;
				cb = qobject_cast<QComboBox*>
					(ui.tableColumns->cellWidget(i, 2));
				cols.append(Utils::q(ui.tableColumns->item(i, 0)->text())
							+ " " + cb->currentText());
			}
		}
		QString sql = QString("create ")
					  + (ui.uniqueCheckBox->isChecked() ? "unique" : "")
					  + " index "
					  + Utils::q(m_schema)
					  + "."
					  + Utils::q(ui.indexNameEdit->text())
					  + " on "
					  + Utils::q(m_table)
					  + " ("
					  + cols.join(", ")
					  + ");";

		QSqlQuery q(sql, QSqlDatabase::database(SESSION_NAME));
		if(q.lastError().isValid())
		{
			resultAppend(tr("Error while creating index ")
						 + ui.indexNameEdit->text()
						 + ":<br/><span style=\" color:#ff0000;\">"
						 + q.lastError().text()
						 + "<br/></span>" + tr("using sql statement:")
						 + "<br/><tt>" + sql);
			return;
		}
		resultAppend(tr("Index created successfully."));
		update = true;
	}
}

void CreateIndexDialog::tableColumns_itemChanged(QTableWidgetItem* item)
{
	int r = item->row();
	Q_ASSERT_X(r != -1 , "CreateIndexDialog", "table item out of the table");
	qobject_cast<QComboBox*>(ui.tableColumns->cellWidget(r, 2))->setEnabled(
							 (item->checkState() == Qt::Checked));
	checkToEnable();
}

void CreateIndexDialog::indexNameEdit_textChanged(const QString &)
{
	checkToEnable();
}

void CreateIndexDialog::checkToEnable()
{
	bool nameTest = !ui.indexNameEdit->text().simplified().isEmpty();
	bool columnTest = false;
	for (int i = 0; i < ui.tableColumns->rowCount(); ++i)
	{
		if (ui.tableColumns->item(i, 1)->checkState() == Qt::Checked)
		{
			columnTest = true;
			break;
		}
	}
	ui.createButton->setEnabled(nameTest && columnTest);
}

void CreateIndexDialog::resultAppend(QString text)
{
	ui.resultEdit->append(text);
	int lh = QFontMetrics(ui.resultEdit->currentFont()).lineSpacing();
	QTextDocument * doc = ui.resultEdit->document();
	if (doc)
	{
		int h = (int)(doc->size().height());
		if (h < lh * 2) { h = lh * 2 + lh / 2; }
		ui.resultEdit->setFixedHeight(h + lh / 2);
	}
	else
	{
		int lines = text.split("<br/>").count() + 1;
		ui.resultEdit->setFixedHeight(lh * lines);
	}
}

void CreateIndexDialog::resizeEvent(QResizeEvent * event)
{
	resizeWanted = true;
	QDialog::resizeEvent(event);
}

void CreateIndexDialog::paintEvent(QPaintEvent * event)
{
	if (resizeWanted)
	{
		Utils::setColumnWidths(ui.tableColumns);
		resizeWanted = false;
	}
	QDialog::paintEvent(event);
}
