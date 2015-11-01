/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
	FIXME Allow editing on views with INSTEAD OF triggers
	FIXME handle things better when not in autocommit mode
	FIXME use explicit string NULL
	FIXME messy column widths
*/

#include <QMessageBox>
#include <QSqlField>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QKeyEvent>
#include <QClipboard>
#include <QDateTime>
#include <QHeaderView>
#include <QResizeEvent>
#include <QSettings>
#include <QInputDialog>
#include <QScrollBar>
#include <QtDebug> //qDebug

#include "dataviewer.h"
#include "preferences.h"
#include "dataexportdialog.h"
#include "sqlmodels.h"
#include "database.h"
#include "sqldelegate.h"
#include "utils.h"
#include "blobpreviewwidget.h"
#include "sqltableview.h"
#include "multieditdialog.h"


DataViewer::DataViewer(QWidget * parent)
	: QMainWindow(parent),
	  dataResized(true)
{
	ui.setupUi(this);
	canFetchMore= tr("(More rows can be fetched. "
		"Scroll the resultset for more rows and/or read the documentation.)");

#ifdef Q_WS_MAC
    ui.mainToolBar->setIconSize(QSize(16, 16));
    ui.exportToolBar->setIconSize(QSize(16, 16));
#endif

	ui.splitter->setCollapsible(0, false);
	ui.splitter->setCollapsible(1, false);
	ui.actionNew_Row->setIcon(Utils::getIcon("insert_table_row.png"));
    ui.actionCopy_Row->setIcon(Utils::getIcon("duplicate_table_row.png"));
	ui.actionRemove_Row->setIcon(Utils::getIcon("delete_table_row.png"));
	ui.actionCommit->setIcon(Utils::getIcon("database_commit.png"));
	ui.actionRollback->setIcon(Utils::getIcon("database_rollback.png"));
	ui.actionRipOut->setIcon(Utils::getIcon("snapshot.png"));
	ui.actionBLOB_Preview->setIcon(Utils::getIcon("blob.png"));
	ui.actionExport_Data->setIcon(Utils::getIcon("document-export.png"));
	ui.actionClose->setIcon(Utils::getIcon("close.png"));
	ui.action_Goto_Line->setIcon(Utils::getIcon("go-next-use.png"));
	ui.actionClose->setVisible(false);
	ui.actionClose->setEnabled(false);
	isTopLevel = true;

	ui.mainToolBar->show();
	ui.exportToolBar->show();
	
	handleBlobPreview(false);

	actInsertNull = new QAction(Utils::getIcon("setnull.png"), tr("Insert NULL"), ui.tableView);
    connect(actInsertNull, SIGNAL(triggered()), this, SLOT(actInsertNull_triggered()));
    actOpenEditor = new QAction(Utils::getIcon("edit.png"), tr("Open Data Editor..."), ui.tableView);
    connect(actOpenEditor, SIGNAL(triggered()), this, SLOT(actOpenEditor_triggered()));
    actOpenMultiEditor = new QAction(tr("Open Multiline Editor..."),
									 ui.tableView);
    connect(actOpenMultiEditor, SIGNAL(triggered()),
			this, SLOT(actOpenMultiEditor_triggered()));
    ui.tableView->addAction(actInsertNull);
    ui.tableView->addAction(actOpenEditor);
    ui.tableView->addAction(actOpenMultiEditor);

	// custom delegate
	SqlDelegate * delegate = new SqlDelegate(this);
	ui.tableView->setItemDelegate(delegate);
	connect(delegate, SIGNAL(dataChanged()),
		this, SLOT(tableView_dataChanged()));


	// workaround for Ctrl+C
	DataViewerTools::KeyPressEater *keyPressEater = new DataViewerTools::KeyPressEater(this);
	ui.tableView->installEventFilter(keyPressEater);

	connect(ui.actionNew_Row, SIGNAL(triggered()),
			this, SLOT(addRow()));
    connect(ui.actionCopy_Row, SIGNAL(triggered()),
            this, SLOT(copyRow()));
	connect(ui.actionRemove_Row, SIGNAL(triggered()),
			this, SLOT(removeRow()));
	connect(ui.actionExport_Data, SIGNAL(triggered()),
			this, SLOT(exportData()));
	connect(ui.actionCommit, SIGNAL(triggered()),
			this, SLOT(commit()));
	connect(ui.actionRollback, SIGNAL(triggered()),
			this, SLOT(rollback()));
	connect(ui.actionRipOut, SIGNAL(triggered()),
			this, SLOT(openStandaloneWindow()));
	connect(ui.actionClose, SIGNAL(triggered()),
			this, SLOT(close()));
	connect(ui.action_Goto_Line, SIGNAL(triggered()),
			this, SLOT(gotoLine()));
	connect(keyPressEater, SIGNAL(copyRequest()),
			this, SLOT(copyHandler()));
// 	connect(parent, SIGNAL(prefsChanged()), ui.tableView, SLOT(repaint()));
	connect(ui.actionBLOB_Preview, SIGNAL(toggled(bool)),
			this, SLOT(handleBlobPreview(bool)));
	connect(ui.tabWidget, SIGNAL(currentChanged(int)),
			this, SLOT(tabWidget_currentChanged(int)));
	connect(ui.tableView->horizontalHeader(), SIGNAL(sectionResized(int, int, int)),
			this, SLOT(tableView_dataResized(int, int, int)));
	connect(ui.tableView->verticalHeader(), SIGNAL(sectionResized(int, int, int)),
			this, SLOT(tableView_dataResized(int, int, int)));
	connect(ui.tableView->verticalHeader(), SIGNAL(sectionDoubleClicked(int)),
			this, SLOT(rowDoubleClicked(int)));
	connect(ui.tableView->verticalScrollBar(), SIGNAL(valueChanged(int)),
					this, SLOT(rowCountChanged()));

	activeRow = -1;
}

DataViewer::~DataViewer()
{
	if (!isTopLevel)
	{
		QSettings settings("yarpen.cz", "sqliteman");
	    settings.setValue("dataviewer/height", QVariant(height()));
	    settings.setValue("dataviewer/width", QVariant(width()));
	}
	freeResources( ui.tableView->model()); // avoid memory leak of model
}

void DataViewer::setNotPending()
{
	SqlTableModel * old = qobject_cast<SqlTableModel*>(ui.tableView->model());
	if (old) { old->setPendingTransaction(false); }
}

bool DataViewer::checkForPending()
{
	SqlTableModel * old = qobject_cast<SqlTableModel*>(ui.tableView->model());
	if (old && old->pendingTransaction())
	{
		QString msg (Database::isAutoCommit()
			? tr("There are unsaved changes in table %1.%2.\n"
				 "Do you wish to commit them to the database?\n\n"
				 "Yes = commit changes\nNo = discard changes\n"
				 "Cancel = skip this operation and stay in %1.%2")
			: tr("There are unsaved changes in table %1.%2.\n"
				 "Do you wish to save them to the database?\n"
				 "(This will not commit as you are in pending"
				 " transaction mode)\n"
				 "\nYes = save changes\nNo = discard changes\n"
				 "Cancel = skip this operation and stay in %1.%2"));
		int com = QMessageBox::question(this, tr("Sqliteman"),
			msg.arg(old->schema(), old->tableName()),
			QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);
		if (com == QMessageBox::No)
		{
			rollback();
			return true;
		}
		else if (com == QMessageBox::Cancel) { return false; }
		else
		{
			if (!old->submitAll())
			{
				/* This should never happen */
				int ret = QMessageBox::question(this, tr("Sqliteman"),
					tr("Failed to write unsaved changes to the database."
					   "\nError: %1\n"
					   "Discard changes?").arg(old->lastError().text()),
												QMessageBox::Yes, QMessageBox::No);
				if (ret == QMessageBox::Yes) { rollback(); }
				else { return false; }
			}
            old->setPendingTransaction(false);
			return true;
		}
	}
	else { return true; }
}

void DataViewer::updateButtons()
{
	int row = -1;
	bool haveRows;
	bool rowSelected = false;
	bool singleItem;
	bool editable;
	bool pending;
	bool canPreview;
	int tab = ui.tabWidget->currentIndex();
	QAbstractItemModel * model = ui.tableView->model();
	SqlTableModel * table = qobject_cast<SqlTableModel *>(model);
	QModelIndexList indexList = ui.tableView->selectedIndexes();
	foreach (const QModelIndex &index, indexList)
	{
		if (index.isValid())
		{
			if (row == -1)
			{
				row = index.row();
				rowSelected = (row >= 0);
				singleItem = rowSelected;
			}
			else
			{
				singleItem = false;
				if (row != index.row())
				{
					rowSelected = false;
				}
			}
		}
	}
	activeRow = rowSelected ? row : -1;
	QVariant data;
	if (model)
	{
		if (table)
		{
			editable = true;
			pending = table->pendingTransaction();
		}
		else
		{
			editable = false;
			pending = false;
		}
		haveRows = model->rowCount() > 0;
		data = model->data(ui.tableView->currentIndex(), Qt::EditRole);
	}
	else
	{
		editable = false;
		pending = false;
		haveRows = false;
	}
	if (singleItem && (data.type() == QVariant::ByteArray))
	{
		QPixmap pm;
		pm.loadFromData(data.toByteArray());
		canPreview = !pm.isNull();
	}
	else
	{
		canPreview = false;
	}
	ui.actionNew_Row->setEnabled(editable);
	ui.actionCopy_Row->setEnabled(editable && rowSelected);
	ui.actionRemove_Row->setEnabled(editable && rowSelected);
	ui.actionCommit->setEnabled(pending);
	ui.actionRollback->setEnabled(pending);
	if (canPreview || ui.actionBLOB_Preview->isChecked())
	{
		ui.actionBLOB_Preview->setEnabled(true);
		ui.blobPreviewBox->setVisible(
			canPreview && ui.actionBLOB_Preview->isChecked());
	}
	else
	{
		ui.actionBLOB_Preview->setEnabled(false);
		ui.blobPreviewBox->setVisible(false);
	}
	ui.actionExport_Data->setEnabled(haveRows);
	ui.action_Goto_Line->setEnabled(haveRows && (tab != 2));
	ui.actionRipOut->setEnabled(haveRows && isTopLevel);
	ui.tabWidget->setTabEnabled(1, rowSelected);
	ui.tabWidget->setTabEnabled(2, ui.scriptEdit->lines() > 1);

	// Sometimes the main toolbar doesn't get displayed. Inserting this line
	// causes it to reappear. Removing the line again does nothing until I've
	// rebuilt the program some indeterminate number of times, and then the
	// main toolbar vanishes again. This is some strange Qt weirdness. End users
	// of the compiled code can't insert the line, rebuild, and remove it again,
	// so it's left in.
	ui.mainToolBar->show();
}

bool DataViewer::setTableModel(QAbstractItemModel * model, bool showButtons)
{
	if (!checkForPending()) { return false; }

	QAbstractItemModel * old = ui.tableView->model();
	ui.tableView->setModel(model); // references old model
	freeResources(old); // avoid memory leak of model

	connect(ui.tableView->selectionModel(),
			SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)),
			this,
			SLOT(tableView_selectionChanged(const QItemSelection &, const QItemSelection &)));
	SqlTableModel * stm = qobject_cast<SqlTableModel*>(model);
	if (stm)
	{
		connect(stm, SIGNAL(reallyDeleting(int)),
				this, SLOT(deletingRow(int)));
	}
	ui.itemView->setModel(model);
	ui.itemView->setTable(ui.tableView);
	ui.tabWidget->setCurrentIndex(0);
	resizeViewToContents(model);
	updateButtons();
	
	rowCountChanged();

	return true;
}

void DataViewer::freeResources(QAbstractItemModel * old)
{
	SqlTableModel * t = qobject_cast<SqlTableModel*>(old);
	if (t)
	{
		SqlTableModel::detach(t);
	}
	else
	{
		SqlQueryModel * q = qobject_cast<SqlQueryModel*>(old);
		if (q)
		{
			SqlQueryModel::detach(q);
		}
	}
}

void DataViewer::saveSelection()
{
	savedActiveRow = activeRow;
	wasItemView = (ui.tabWidget->currentIndex() == 1);
}

void DataViewer::reSelect()
{
	if (savedActiveRow >= 0)
	{
		ui.tableView->selectRow(savedActiveRow);
		if (wasItemView)
		{
			ui.tabWidget->setCurrentIndex(1);
			ui.itemView->setCurrentIndex(ui.tableView->currentIndex().row(),
										 ui.tableView->currentIndex().column());
		}
	}
}

void DataViewer::tableView_dataResized(int column, int oldWidth, int newWidth) 
{
	dataResized = true;
}

void DataViewer::resizeEvent(QResizeEvent * event)
{
	if (!dataResized && ui.tableView->model())
		resizeViewToContents(ui.tableView->model());
}


void DataViewer::resizeViewToContents(QAbstractItemModel * model)
{
	if (model->columnCount() <= 0)
		return;

	ui.tableView->resizeColumnsToContents();
	ui.tableView->resizeRowsToContents();

	int total = 0;
	for (int i = 0; i < model->columnCount(); ++i)
		total += ui.tableView->columnWidth(i);

	if (total < ui.tableView->viewport()->width())
	{
		int extra = (ui.tableView->viewport()->width() - total)
			/ model->columnCount();
		for (int i = 0; i < model->columnCount(); ++i)
			ui.tableView->setColumnWidth(i, ui.tableView->columnWidth(i) + extra);
	}
	dataResized = false;
}

void DataViewer::setStatusText(const QString & text)
{
	ui.statusText->setHtml(text);
	int lh = QFontMetrics(ui.statusText->currentFont()).lineSpacing();
	QTextDocument * doc = ui.statusText->document();
	if (doc)
	{
		int h = (int)(doc->size().height());
		if (h < lh * 2) { h = lh * 2 + lh / 2; }
		ui.statusText->setFixedHeight(h + lh / 2);
	}
	else
	{
		int lines = text.split("<br/>").count() + 1;
		ui.statusText->setFixedHeight(lh * lines);
	}
	showStatusText(true);
}

void DataViewer::removeErrorMessage()
{
	QString s = ui.statusText->toHtml();
	if (s.contains("<span style=\" color:#ff0000;\">"))
	{
		ui.statusText->setPlainText("");
	}
}

void DataViewer::showStatusText(bool show)
{
	(show) ? ui.statusText->show() : ui.statusText->hide();
}

void DataViewer::addRow()
{
	// FIXME adding new row with INTEGER PRIMARY KEY doesn't fill it in
	removeErrorMessage();
	SqlTableModel * model
		= qobject_cast<SqlTableModel *>(ui.tableView->model());
	if (model)
	{
		activeRow = model->rowCount();
		model->insertRows(activeRow, 1);
		ui.tableView->scrollToBottom();
		ui.tableView->selectRow(activeRow);
		updateButtons();
		if (ui.tabWidget->currentIndex() == 1)
		{
			ui.itemView->setCurrentIndex(
				ui.tableView->currentIndex().row(),
				ui.tableView->currentIndex().column());
		}
	}
}

void DataViewer::copyRow()
{
    SqlTableModel * model =
	    qobject_cast<SqlTableModel *>(ui.tableView->model());
    if (model)
    {
        int row = ui.tableView->currentIndex().row();
        if (row >= 0)
        {
            QSqlRecord rec(model->record(row));
			/* We should be able to insert the copied row after the current one,
			 * but there seems to be some kind of strange bug in QSqlTableModel
			 * such that if we do so, we get a reference to the same record
			 * rather than a new different one.
			 */
            if (model->insertRecord(-1, rec))
			{
				ui.tableView->selectRow(model->rowCount() - 1);
				updateButtons();
				if (ui.tabWidget->currentIndex() == 1)
				{
					ui.itemView->setCurrentIndex(
						ui.tableView->currentIndex().row(),
						ui.tableView->currentIndex().column());
				}
			}
        }
    }
}

void DataViewer::removeRow()
{
	removeErrorMessage();
	SqlTableModel * model =
		qobject_cast<SqlTableModel *>(ui.tableView->model());
	if (model)
	{
		int row = ui.tableView->currentIndex().row();
		model->removeRows(row, 1);
		ui.tableView->hideRow(row);
		if (ui.tabWidget->currentIndex() == 1)
		{
			if (ui.itemView->rowDeleted())
			{
				// no rows left
				ui.tabWidget->setCurrentIndex(0);
			}
		}
		updateButtons();
	}
}

void DataViewer::deletingRow(int row)
{
	if ((row <= savedActiveRow) && (savedActiveRow > 0)) { --savedActiveRow; }
}

void DataViewer::exportData()
{
	removeErrorMessage();
	QString tmpTableName("<any_table>");
	SqlTableModel * m = qobject_cast<SqlTableModel*>(ui.tableView->model());
	if (m)
		tmpTableName = m->tableName();

	DataExportDialog *dia = new DataExportDialog(this, tmpTableName);
	if (dia->exec())
		if (!dia->doExport())
			QMessageBox::warning(this, tr("Export Error"), tr("Data export failed"));
	delete dia;
}

QSqlQueryModel* DataViewer::tableData()
{
	return qobject_cast<QSqlQueryModel *>(ui.tableView->model());
}

QStringList DataViewer::tableHeader()
{
	QStringList ret;
	QSqlQueryModel *q = qobject_cast<QSqlQueryModel *>(ui.tableView->model());

	for (int i = 0; i < q->columnCount() ; ++i)
		ret << q->headerData(i, Qt::Horizontal).toString();

	return ret;
}

void DataViewer::commit()
{
	removeErrorMessage();
	saveSelection();
	// HACK: some Qt4 versions crash on commit/rollback when there
	// is a new - currently edited - row in a transaction. This
	// forces to close the editor/delegate.
	ui.tableView->selectRow(ui.tableView->currentIndex().row());
	SqlTableModel * model = qobject_cast<SqlTableModel *>(ui.tableView->model());
	if (!model->submitAll())
	{
		int ret = QMessageBox::question(this, tr("Sqliteman"),
				tr("There is a pending transaction in progress."
				   " That cannot be committed now."
				   "\nError: %1\n"
				   "Perform rollback?").arg(model->lastError().text()),
				QMessageBox::Yes, QMessageBox::No);
		if(ret == QMessageBox::Yes)
		{
			rollback();
		}
		return;
	}
	model->setPendingTransaction(false);
	reSelect();
	resizeViewToContents(model);
	updateButtons();
}

void DataViewer::rollback()
{
	removeErrorMessage();
	saveSelection();
	// HACK: some Qt4 versions crash on commit/rollback when there
	// is a new - currently edited - row in a transaction. This
	// forces to close the editor/delegate.
	ui.tableView->selectRow(ui.tableView->currentIndex().row());
	SqlTableModel * model = qobject_cast<SqlTableModel *>(ui.tableView->model());
	if (model) // paranoia
	{
		model->revertAll();
		model->setPendingTransaction(false);
		int n = model->rowCount();
		for (int i = 0; i < n; ++i)
		{
			ui.tableView->showRow(i);
		}
		reSelect();
		resizeViewToContents(model);
		updateButtons();
	}
}

void DataViewer::copyHandler()
{
	removeErrorMessage();
	QItemSelectionModel *selectionModel = ui.tableView->selectionModel();
	QModelIndexList selectedIndexes = selectionModel->selectedIndexes();
	QModelIndex index;
	// This looks very "pythonic" maybe there is better way to do...
	QMap<int,QMap<int,QString> > snapshot;
	QStringList out;

	foreach (index, selectedIndexes)
		snapshot[index.row()][index.column()] = index.data().toString();
	
	QMapIterator<int,QMap<int,QString> > it(snapshot);
	while (it.hasNext())
	{
		it.next();
		QMapIterator<int,QString> j(it.value());
		while (j.hasNext())
		{
			j.next();
			out << j.value();
			if (j.hasNext())
				out << "\t";
		}
		out << "\n";
	}

	if (out.size() != 0)
		QApplication::clipboard()->setText(out.join(QString::null));
}

void DataViewer::openStandaloneWindow()
{
	removeErrorMessage();
	SqlTableModel *tm = qobject_cast<SqlTableModel*>(ui.tableView->model());

#ifdef WIN32
    // win windows are always top when there is this parent
    DataViewer *w = new DataViewer(0);
#else
    DataViewer *w = new DataViewer(this);
#endif
	SqlQueryModel *qm = new SqlQueryModel(w);
	w->setAttribute(Qt::WA_DeleteOnClose);
	w->isTopLevel = false;
	QSettings settings("yarpen.cz", "sqliteman");
	int hh = settings.value("dataviewer/height", QVariant(607)).toInt();
	int ww = settings.value("dataviewer/width", QVariant(819)).toInt();
	w->resize(ww, hh);
	

	//! TODO: change setWindowTitle() to the unified QString().arg() sequence after string unfreezing
	if (tm)
	{
		w->setWindowTitle(tm->tableName() + " - "
				+ QDateTime::currentDateTime().toString() + " - " 
				+ tr("Data Snapshot"));
		qm->setQuery(QString("select * from ")
					 + Utils::quote(tm->schema())
					 + "."
					 + Utils::quote(tm->tableName())
					 + ";",
					 QSqlDatabase::database(SESSION_NAME));
	}
	else
	{
		w->setWindowTitle("SQL - "
				+ QDateTime::currentDateTime().toString() + " - " 
				+ tr("Data Snapshot"));
		QSqlQueryModel * m = qobject_cast<QSqlQueryModel*>(ui.tableView->model());
		qm->setQuery(m->query());
	}

	qm->attach();
	w->setTableModel(qm);
	w->ui.mainToolBar->hide();
	w->ui.actionRipOut->setEnabled(false);
	w->ui.actionClose->setEnabled(true);
	w->ui.actionClose->setVisible(true);
	w->ui.tabWidget->removeTab(2);
	w->show();
	w->setStatusText(tr("%1 snapshot for: %2")
		.arg("<tt>"+QDateTime::currentDateTime().toString()+"</tt><br/>")
		.arg("<br/><tt>" + qm->query().lastQuery())+ "</tt>");
}

void DataViewer::handleBlobPreview(bool state)
{
	if (state)
		tableView_selectionChanged(QItemSelection(), QItemSelection());
	updateButtons();
	if (ui.blobPreviewBox->isVisible())
	{
		ui.blobPreview->setBlobData(
			ui.tableView->model()->data(ui.tableView->currentIndex(),
										Qt::EditRole));
	}
}

void DataViewer::tableView_selectionChanged(const QItemSelection &,
											const QItemSelection &)
{
	removeErrorMessage();
	SqlTableModel *tm = qobject_cast<SqlTableModel*>(ui.tableView->model());
    bool enable = (tm != 0);
    actInsertNull->setEnabled(enable);
    actOpenEditor->setEnabled(enable);
    actOpenMultiEditor->setEnabled(enable);

	updateButtons();
	QModelIndex index = ui.tableView->currentIndex();
	if (ui.blobPreviewBox->isVisible())
	{
		if (index.isValid())
		{
			ui.blobPreview->setBlobData(
				ui.tableView->model()->data(index, Qt::EditRole));
		}
		else
		{
			ui.blobPreview->setBlobData(QVariant());
		}
	}
}

void DataViewer::tabWidget_currentChanged(int ix)
{
	removeErrorMessage();
	if (ix == 0)
	{
		// be careful with this. See itemView_indexChanged() docs.
		disconnect(ui.itemView, SIGNAL(indexChanged()),
				   this, SLOT(itemView_indexChanged()));
		disconnect(ui.itemView, SIGNAL(dataChanged()),
				   this, SLOT(tableView_dataChanged()));
	}
	if (ix == 1)
	{
		ui.itemView->setCurrentIndex(ui.tableView->currentIndex().row(),
									 ui.tableView->currentIndex().column());
		// be careful with this. See itemView_indexChanged() docs.
		connect(ui.itemView, SIGNAL(indexChanged()),
				this, SLOT(itemView_indexChanged()));
		connect(ui.itemView, SIGNAL(dataChanged()),
				this, SLOT(tableView_dataChanged()));
	}
	
	if (ui.actionBLOB_Preview->isChecked())
		ui.blobPreviewBox->setVisible(ix!=2);
	ui.statusText->setVisible(ix != 2);
	updateButtons();
}

void DataViewer::itemView_indexChanged()
{
	removeErrorMessage();
	ui.tableView->setCurrentIndex(
		ui.tableView->model()->index(ui.itemView->currentRow(),
								     ui.itemView->currentColumn())
							);
	updateButtons();
}

void DataViewer::tableView_dataChanged()
{
	removeErrorMessage();
	updateButtons();
}

void DataViewer::showSqlScriptResult(QString line)
{
	removeErrorMessage();
	if (line.isEmpty()) { return; }
	ui.scriptEdit->append(line);
	ui.scriptEdit->append("\n");
	ui.scriptEdit->ensureLineVisible(ui.scriptEdit->lines());
	ui.tabWidget->setCurrentIndex(2);
	updateButtons();
}

void DataViewer::sqlScriptStart()
{
	ui.scriptEdit->clear();
}

void DataViewer::gotoLine()
{
	removeErrorMessage();
	bool ok;
	int row = QInputDialog::getInt(this, tr("Goto Line"), tr("Goto Line:"),
								   ui.tableView->currentIndex().row(), // value
								   1, // min
								   ui.tableView->model()->rowCount(), // max (no fetchMore loop)
								   1, // step
								   &ok);
	if (!ok)
		return;

	QModelIndex left;
	SqlTableModel * model = qobject_cast<SqlTableModel *>(ui.tableView->model());
	int column = ui.tableView->currentIndex().isValid() ? ui.tableView->currentIndex().column() : 0;
	row -= 1;

	if (model)
		left = model->createIndex(row, column);
	else
	{
		SqlQueryModel * model = qobject_cast<SqlQueryModel *>(ui.tableView->model());
		if (model)
			left = model->createIndex(row, column);
	}

	ui.tableView->selectionModel()->select(QItemSelection(left, left),
										   QItemSelectionModel::ClearAndSelect);
	ui.tableView->setCurrentIndex(left);
	if (ui.tabWidget->currentIndex() == 1)
	{
		ui.itemView->setCurrentIndex(row, column);
	}
	updateButtons();
}

void DataViewer::actOpenEditor_triggered()
{
	removeErrorMessage();
    ui.tableView->edit(ui.tableView->currentIndex());
}

void DataViewer::actOpenMultiEditor_triggered()
{
	removeErrorMessage();
	MultiEditDialog * dia = new MultiEditDialog(this);
	QAbstractItemModel * model = ui.tableView->model();
	QVariant data = model->data(ui.tableView->currentIndex(), Qt::EditRole);
	dia->setData(data);
	if (dia->exec())
	{
		data = dia->data();
		ui.tableView->model()->setData(ui.tableView->currentIndex(),
									   data, Qt::EditRole);
	}
}

void DataViewer::actInsertNull_triggered()
{
	removeErrorMessage();
    ui.tableView->model()->setData(ui.tableView->currentIndex(), QString(), Qt::EditRole); 
}

void DataViewer::rowDoubleClicked(int)
{
	removeErrorMessage();
	ui.tabWidget->setCurrentIndex(1);
}

void DataViewer::rowCountChanged()
{
	removeErrorMessage();
	QString cached;
	QSqlQueryModel * model = qobject_cast<QSqlQueryModel*>(ui.tableView->model());
	if ((model != 0) && (model->columnCount() > 0))
	{
		if(   (model->rowCount() != 0)
		   && model->canFetchMore())
	    {
			cached = canFetchMore + "<br/>";
	    }
	    else { cached = ""; }

		setStatusText(tr("Query OK<br/>Row(s) returned: %1 %2")
					  .arg(model->rowCount()).arg(cached));
	}
	else { showStatusText(false); }
}

/* Tools *************************************************** */

bool DataViewerTools::KeyPressEater::eventFilter(QObject *obj, QEvent *event)
{
	if (event->type() == QEvent::KeyPress)
	{
		QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
		if (keyEvent == QKeySequence::Copy)
		{
			emit copyRequest();
			return true;
		}
		return QObject::eventFilter(obj, event);
	}
	else
	{
		// standard event processing
		return QObject::eventFilter(obj, event);
	}
}


