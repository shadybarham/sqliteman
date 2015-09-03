/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
	FIXME temporary table fails to display
	FIXME add function to evaluate an expression

*/
#include <QTreeWidget>
#include <QTableView>
#include <QSplitter>
#include <QMenuBar>
#include <QMenu>
#include <QTime>

#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include <QCoreApplication>
#include <QCloseEvent>
#include <QSettings>
#include <QFileInfo>
#include <QAction>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QtDebug>

#include "litemanwindow.h"
#include "preferences.h"
#include "preferencesdialog.h"
#include "queryeditordialog.h"
#include "createtabledialog.h"
#include "createtriggerdialog.h"
#include "createviewdialog.h"
#include "alterviewdialog.h"
#include "altertabledialog.h"
#include "altertriggerdialog.h"
#include "dataviewer.h"
#include "schemabrowser.h"
#include "database.h"
#include "sqleditor.h"
#include "sqlmodels.h"
#include "createindexdialog.h"
#include "constraintsdialog.h"
#include "analyzedialog.h"
#include "vacuumdialog.h"
#include "helpbrowser.h"
#include "importtabledialog.h"
#include "sqliteprocess.h"
#include "populatordialog.h"
#include "utils.h"
#include "buildtime.h"

#ifdef INTERNAL_SQLDRIVER
#include "driver/qsql_sqlite.h"
#endif



LiteManWindow::LiteManWindow(const QString & fileToOpen)
	: QMainWindow(),
	m_mainDbPath(""),
	m_appName("Sqliteman"),
// 	m_sqliteBinAvailable(true),
	helpBrowser(0)
{
	// test for sqlite3 binary
/*	qDebug() << "Checking for " << SQLITE_BINARY << ":";
	if (QProcess::execute(SQLITE_BINARY, QStringList() << "-version") != 0)
	{
		m_sqliteBinAvailable = false;
		QMessageBox::warning(this, m_appName,
							 tr("Sqlite3 executable '%1' is not found in your path. Some features will be disabled.")
									 .arg(SQLITE_BINARY));
		qDebug() << "Sqlite3 executable '%1' is not found in your path. Some features will be disabled.";
	}*/
	qDebug() << "Checking for Qt version: " << qVersion();
#if QT_VERSION < 0x040300
	if (Preferences::instance()->checkQtVersion())
	{
		QMessageBox::warning(this, m_appName,
						 tr("Sqliteman is using Qt %1. Some features will be disabled.")
							.arg(qVersion()));
		qDebug() << "Sqliteman is using Qt %1. Some features will be disabled.";
	}
#endif

	recentDocs.clear();
	attachedDb.clear();
	initUI();
	initActions();
	initMenus();

	statusBar();
	m_sqliteVersionLabel = new QLabel(this);
	m_activeItem = 0;
	statusBar()->addPermanentWidget(m_sqliteVersionLabel);

	readSettings();

	// Check command line
	qDebug() << "Initial DB: " << fileToOpen;
	if (!fileToOpen.isNull() && !fileToOpen.isEmpty())
		open(fileToOpen);
}

LiteManWindow::~LiteManWindow()
{
	Preferences::deleteInstance();
}

bool LiteManWindow::checkForPending() {
	return dataViewer->checkForPending();
}

void LiteManWindow::closeEvent(QCloseEvent * e)
{
	if (!sqlEditor->saveOnExit()) {
		e->ignore ();
	}
	
	// check for uncommitted transaction in the DB
	if (!dataViewer->checkForPending())
	{
		e->ignore();
		return;
	}

	writeSettings();

	QMapIterator<QString, QString> i(attachedDb);
	while (i.hasNext())
	{
		i.next();
		QSqlDatabase::database(i.value()).rollback();
		QSqlDatabase::database(i.value()).close();
		QSqlDatabase::removeDatabase(i.value());
	}

	// It has to go after writeSettings()!
	//foreach (QWidget *widget, QApplication::topLevelWidgets())
	//	widget->close();

	e->accept();
}


void LiteManWindow::initUI()
{
	setWindowTitle(m_appName);
	splitter = new QSplitter(this);
	splitterSql = new QSplitter(Qt::Vertical, this);

	schemaBrowser = new SchemaBrowser(this);
	dataViewer = new DataViewer(this);
	sqlEditor = new SqlEditor(this);

	splitterSql->addWidget(sqlEditor);
	splitterSql->addWidget(dataViewer);
	splitterSql->setCollapsible(0, false);
	splitterSql->setCollapsible(1, false);

	splitter->addWidget(schemaBrowser);
	splitter->addWidget(splitterSql);
	splitter->setCollapsible(0, false);
	splitter->setCollapsible(1, false);

	setCentralWidget(splitter);

	// Disable the UI, as long as there is no open database
	schemaBrowser->setEnabled(false);
	dataViewer->setEnabled(false);
	sqlEditor->setEnabled(false);
	sqlEditor->hide();

	connect(schemaBrowser->tableTree, SIGNAL(itemActivated(QTreeWidgetItem *, int)),
		this, SLOT(treeItemActivated(QTreeWidgetItem *, int)));

	connect(schemaBrowser->tableTree, SIGNAL(customContextMenuRequested(const QPoint &)),
		this, SLOT(treeContextMenuOpened(const QPoint &)));
	connect(schemaBrowser->tableTree, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
			this, SLOT(tableTree_currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)));

	// sql editor
	connect(sqlEditor, SIGNAL(showSqlResult(QString)),
			this, SLOT(execSql(QString)));
	connect(sqlEditor, SIGNAL(sqlScriptStart()),
			dataViewer, SLOT(sqlScriptStart()));
	connect(sqlEditor, SIGNAL(showSqlScriptResult(QString)),
			dataViewer, SLOT(showSqlScriptResult(QString)));
	connect(sqlEditor, SIGNAL(rebuildViewTree(QString, QString)),
			schemaBrowser->tableTree, SLOT(buildViewTree(QString,QString)));
	connect(sqlEditor, SIGNAL(buildTree()),
			schemaBrowser->tableTree, SLOT(buildTree()));
	connect(sqlEditor, SIGNAL(refreshTable()),
			this, SLOT(refreshTable()));
}

void LiteManWindow::initActions()
{
	newAct = new QAction(Utils::getIcon("document-new.png"),
						 tr("&New..."), this);
	newAct->setShortcut(tr("Ctrl+N"));
	connect(newAct, SIGNAL(triggered()), this, SLOT(newDB()));

	openAct = new QAction(Utils::getIcon("document-open.png"),
						  tr("&Open..."), this);
	openAct->setShortcut(tr("Ctrl+O"));
	connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

	recentAct = new QAction(tr("&Recent Databases"), this);
	recentFilesMenu = new QMenu(this);
	recentAct->setMenu(recentFilesMenu);

	preferencesAct = new QAction(tr("&Preferences..."), this);
	connect(preferencesAct, SIGNAL(triggered()), this, SLOT(preferences()));

	exitAct = new QAction(Utils::getIcon("close.png"), tr("E&xit"), this);
	exitAct->setShortcut(tr("Ctrl+Q"));
	connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

	aboutAct = new QAction(tr("&About..."), this);
	connect(aboutAct, SIGNAL(triggered()), this, SLOT(about()));

	aboutQtAct = new QAction(tr("About &Qt..."), this);
	connect(aboutQtAct, SIGNAL(triggered()), this, SLOT(aboutQt()));

	helpAct = new QAction(tr("&Help Content..."), this);
	helpAct->setShortcut(tr("F1"));
	connect(helpAct, SIGNAL(triggered()), this, SLOT(help()));

	execSqlAct = new QAction(tr("&SQL Editor"), this);
	execSqlAct->setShortcut(tr("Ctrl+E"));
	execSqlAct->setCheckable(true);
	connect(execSqlAct, SIGNAL(triggered()), this, SLOT(handleSqlEditor()));

	objectBrowserAct = new QAction(tr("Object &Browser"), this);
	objectBrowserAct->setShortcut(tr("Ctrl+B"));
	objectBrowserAct->setCheckable(true);
	connect(objectBrowserAct, SIGNAL(triggered()), this, SLOT(handleObjectBrowser()));

	dataViewerAct = new QAction(tr("Data Vie&wer"), this);
	dataViewerAct->setShortcut(tr("Ctrl+D"));
	dataViewerAct->setCheckable(true);
	connect(dataViewerAct, SIGNAL(triggered()), this, SLOT(handleDataViewer()));

	buildQueryAct = new QAction(tr("&Build Query..."), this);
	buildQueryAct->setShortcut(tr("Ctrl+R"));
	connect(buildQueryAct, SIGNAL(triggered()), this, SLOT(buildQuery()));

	contextBuildQueryAct = new QAction(tr("&Build Query..."), this);
	connect(contextBuildQueryAct, SIGNAL(triggered()),
			this, SLOT(contextBuildQuery()));

	exportSchemaAct = new QAction(tr("&Export Schema..."), this);
	connect(exportSchemaAct, SIGNAL(triggered()), this, SLOT(exportSchema()));

	dumpDatabaseAct = new QAction(tr("&Dump Database..."), this);
	connect(dumpDatabaseAct, SIGNAL(triggered()), this, SLOT(dumpDatabase()));
// 	dumpDatabaseAct->setEnabled(m_sqliteBinAvailable);

	createTableAct = new QAction(Utils::getIcon("table.png"),
								 tr("&Create Table..."), this);
	createTableAct->setShortcut(tr("Ctrl+T"));
	connect(createTableAct, SIGNAL(triggered()), this, SLOT(createTable()));

	dropTableAct = new QAction(tr("&Drop Table"), this);
	connect(dropTableAct, SIGNAL(triggered()), this, SLOT(dropTable()));

	alterTableAct = new QAction(tr("&Alter Table..."), this);
	alterTableAct->setShortcut(tr("Ctrl+L"));
	connect(alterTableAct, SIGNAL(triggered()), this, SLOT(alterTable()));

	renameTableAct = new QAction(tr("&Rename Table..."), this);
	connect(renameTableAct, SIGNAL(triggered()), this, SLOT(renameTable()));

	populateTableAct = new QAction(tr("&Populate Table..."), this);
	connect(populateTableAct, SIGNAL(triggered()), this, SLOT(populateTable()));

	createViewAct = new QAction(Utils::getIcon("view.png"),
								tr("Create &View..."), this);
	createViewAct->setShortcut(tr("Ctrl+G"));
	connect(createViewAct, SIGNAL(triggered()), this, SLOT(createView()));

	dropViewAct = new QAction(tr("&Drop View"), this);
	connect(dropViewAct, SIGNAL(triggered()), this, SLOT(dropView()));

	alterViewAct = new QAction(tr("&Alter View..."), this);
	connect(alterViewAct, SIGNAL(triggered()), this, SLOT(alterView()));

	createIndexAct = new QAction(Utils::getIcon("index.png"),
								 tr("&Create Index..."), this);
	connect(createIndexAct, SIGNAL(triggered()), this, SLOT(createIndex()));

	dropIndexAct = new QAction(tr("&Drop Index"), this);
	connect(dropIndexAct, SIGNAL(triggered()), this, SLOT(dropIndex()));

	describeTableAct = new QAction(tr("D&escribe Table"), this);
	connect(describeTableAct, SIGNAL(triggered()), this, SLOT(describeObject()/*describeTable()*/));

	importTableAct = new QAction(tr("&Import Table Data..."), this);
	connect(importTableAct, SIGNAL(triggered()), this, SLOT(importTable()));

	createTriggerAct = new QAction(Utils::getIcon("trigger.png"),
								   tr("&Create Trigger..."), this);
	connect(createTriggerAct, SIGNAL(triggered()), this, SLOT(createTrigger()));

	alterTriggerAct = new QAction(tr("&Alter Trigger..."), this);
	connect(alterTriggerAct, SIGNAL(triggered()), this, SLOT(alterTrigger()));

	dropTriggerAct = new QAction(tr("&Drop Trigger"), this);
	connect(dropTriggerAct, SIGNAL(triggered()), this, SLOT(dropTrigger()));

	describeTriggerAct = new QAction(tr("D&escribe Trigger"), this);
	connect(describeTriggerAct, SIGNAL(triggered()), this, SLOT(describeObject()/*describeTrigger()*/));

	describeViewAct = new QAction(tr("D&escribe View"), this);
	connect(describeViewAct, SIGNAL(triggered()), this, SLOT(describeObject()/*describeView()*/));

	describeIndexAct = new QAction(tr("D&escribe Index"), this);
	connect(describeIndexAct, SIGNAL(triggered()), this, SLOT(describeObject()/*describeIndex()*/));

	reindexAct = new QAction(tr("&Reindex"), this);
	connect(reindexAct, SIGNAL(triggered()), this, SLOT(reindex()));

	analyzeAct = new QAction(tr("&Analyze Statistics..."), this);
	connect(analyzeAct, SIGNAL(triggered()), this, SLOT(analyzeDialog()));

	vacuumAct = new QAction(tr("&Vacuum..."), this);
	connect(vacuumAct, SIGNAL(triggered()), this, SLOT(vacuumDialog()));

	attachAct = new QAction(tr("A&ttach Database..."), this);
	connect(attachAct, SIGNAL(triggered()), this, SLOT(attachDatabase()));

	detachAct = new QAction(tr("&Detach Database"), this);
	connect(detachAct, SIGNAL(triggered()), this, SLOT(detachDatabase()));

#ifdef ENABLE_EXTENSIONS
	loadExtensionAct = new QAction(tr("&Load Extensions..."), this);
	connect(loadExtensionAct, SIGNAL(triggered()), this, SLOT(loadExtension()));
#endif

	refreshTreeAct = new QAction(tr("&Refresh Object Tree"), this);
	connect(refreshTreeAct, SIGNAL(triggered()), schemaBrowser->tableTree, SLOT(buildTree()));

	consTriggAct = new QAction(tr("&Constraint Triggers..."), this);
	connect(consTriggAct, SIGNAL(triggered()), this, SLOT(constraintTriggers()));
}

void LiteManWindow::initMenus()
{
	QMenu * fileMenu = menuBar()->addMenu(tr("&File"));
	fileMenu->addAction(newAct);
	fileMenu->addAction(openAct);
	fileMenu->addAction(recentAct);
	fileMenu->addSeparator();
	fileMenu->addAction(preferencesAct);
	fileMenu->addSeparator();
	fileMenu->addAction(exitAct);

	contextMenu = menuBar()->addMenu(tr("&Context"));
	contextMenu->setEnabled(false);

	databaseMenu = menuBar()->addMenu(tr("&Database"));
	databaseMenu->addAction(createTableAct);
	databaseMenu->addAction(createViewAct);
	databaseMenu->addSeparator();
	databaseMenu->addAction(buildQueryAct);
	databaseMenu->addAction(execSqlAct);
	databaseMenu->addAction(objectBrowserAct);
	databaseMenu->addAction(dataViewerAct);
	databaseMenu->addSeparator();
	databaseMenu->addAction(exportSchemaAct);
	databaseMenu->addAction(dumpDatabaseAct);
	databaseMenu->addAction(importTableAct);

	adminMenu = menuBar()->addMenu(tr("&System"));
	adminMenu->addAction(analyzeAct);
	adminMenu->addAction(vacuumAct);
	adminMenu->addSeparator();
	adminMenu->addAction(attachAct);
#ifdef ENABLE_EXTENSIONS
	adminMenu->addSeparator();
	adminMenu->addAction(loadExtensionAct);
#endif

	QMenu * helpMenu = menuBar()->addMenu(tr("&Help"));
	helpMenu->addAction(helpAct);
	helpMenu->addAction(aboutAct);
	helpMenu->addAction(aboutQtAct);

	databaseMenu->setEnabled(false);
	adminMenu->setEnabled(false);
}

void LiteManWindow::updateRecent(QString fn)
{
	if (recentDocs.indexOf(fn) == -1)
		recentDocs.prepend(fn);
	else
		recentDocs.replace(recentDocs.indexOf(fn), fn);
	rebuildRecentFileMenu();
}

void LiteManWindow::removeRecent(QString fn)
{
	if (recentDocs.indexOf(fn) != -1)
		recentDocs.removeAt(recentDocs.indexOf(fn));
	rebuildRecentFileMenu();
}

void LiteManWindow::rebuildRecentFileMenu()
{
	recentFilesMenu->clear();
	uint max = qMin(Preferences::instance()->recentlyUsedCount(), recentDocs.count());
	QFile fi;
	QString accel("&");

	for (uint i = 0; i < max; ++i)
	{
		fi.setFileName(recentDocs.at(i));
		if (!fi.exists())
		{
			removeRecent(recentDocs.at(i));
			break;
		}
		// &10 collides with &1
		if (i > 8)
			accel = "";
		QAction *a = new QAction(accel
								 + QString('1' + i)
								 + " "
								 + recentDocs.at(i),
								 this);
		a->setData(QVariant(recentDocs.at(i)));
		connect(a, SIGNAL(triggered()), this, SLOT(openRecent()));
		recentFilesMenu->addAction(a);
	}
	recentAct->setDisabled(recentDocs.count() == 0);
}

void LiteManWindow::readSettings()
{
	QSettings settings("yarpen.cz", "sqliteman");

	int hh = settings.value("litemanwindow/height", QVariant(600)).toInt();
	int ww = settings.value("litemanwindow/width", QVariant(800)).toInt();
	resize(ww, hh);
	QByteArray splitterData = settings.value("window/splitter").toByteArray();

	splitter->restoreState(splitterData);
	splitterSql->restoreState(settings.value("sqleditor/splitter").toByteArray());

	dataViewer->restoreSplitter(settings.value("dataviewer/splitter").toByteArray());
	dataViewer->setVisible(settings.value("dataviewer/show", true).toBool());
	dataViewerAct->setChecked(settings.value("dataviewer/show", true).toBool());

	sqlEditor->setVisible(settings.value("sqleditor/show", true).toBool());
	schemaBrowser->setVisible(settings.value("objectbrowser/show", true).toBool());
	objectBrowserAct->setChecked(settings.value("objectbrowser/show", true).toBool());
	execSqlAct->setChecked(settings.value("sqleditor/show", true).toBool());

	QString fn(settings.value("sqleditor/filename", QString()).toString());
	if (!fn.isNull() && !fn.isEmpty() && Preferences::instance()->openLastSqlFile())
	   sqlEditor->setFileName(fn);

	recentDocs = settings.value("recentDocs/files").toStringList();
	rebuildRecentFileMenu();
}

void LiteManWindow::writeSettings()
{
	QSettings settings("yarpen.cz", "sqliteman");

    settings.setValue("litemanwindow/height", QVariant(height()));
    settings.setValue("litemanwindow/width", QVariant(width()));
	settings.setValue("window/splitter", splitter->saveState());
	settings.setValue("objectbrowser/show", schemaBrowser->isVisible());
	settings.setValue("sqleditor/show", sqlEditor->isVisible());
	settings.setValue("sqleditor/splitter", splitterSql->saveState());
	settings.setValue("sqleditor/filename", sqlEditor->fileName());
	settings.setValue("dataviewer/show", dataViewer->isVisible());
	settings.setValue("dataviewer/splitter", dataViewer->saveSplitter());
	settings.setValue("recentDocs/files", recentDocs);
	// last open database
	settings.setValue("lastDatabase",
					  QSqlDatabase::database(SESSION_NAME).databaseName());
}

void LiteManWindow::newDB()
{
	// Creating a database is virtually the same as opening an existing one. SQLite simply creates
	// a database which doesn't already exist
	QString fileName = QFileDialog::getSaveFileName(this, tr("New Database"), QDir::currentPath(), tr("SQLite database (*)"));

	if(fileName.isNull())
		return;

	if (QFile::exists(fileName))
		QFile::remove(fileName);

	openDatabase(fileName);
}

void LiteManWindow::open(const QString & file)
{
	QString fileName;

	// If no file name was provided, open dialog
	if(!file.isNull())
		fileName = file;
	else
		fileName = QFileDialog::getOpenFileName(this,
								tr("Open Database"),
								QDir::currentPath(),
								tr("SQLite database (*)"));

	if(fileName.isNull())
		return;
	openDatabase(fileName);
}

void LiteManWindow::openDatabase(const QString & fileName)
{
	if (!checkForPending()) { return; }
	
	bool isOpened = false;

	QSqlDatabase db = QSqlDatabase::database(SESSION_NAME);
	if (db.isValid())
	{
		db.close();
		QSqlDatabase::removeDatabase(SESSION_NAME);
	}
#ifdef INTERNAL_SQLDRIVER
	db = QSqlDatabase::addDatabase(new QSQLiteDriver(this), SESSION_NAME);
#else
	db = QSqlDatabase::addDatabase("QSQLITE", SESSION_NAME);
#endif

	db.setDatabaseName(fileName);

	if (!db.open())
	{
		dataViewer->setStatusText(tr("Cannot open or create ")
								  + QFileInfo(fileName).fileName()
								  + ":<br/>"
								  + db.lastError().text());
		return;
	}
	/* Qt database open() (exactly the sqlite API sqlite3_open16())
	   method does not check if it is really database. So the dummy
	   select statement should perform a real "is it a database" check
	   for us. */
	QSqlQuery q("select 1 from sqlite_master where 1=2", db);
	if (!q.exec())
	{
		dataViewer->setStatusText(tr("Cannot access ")
								  + QFileInfo(fileName).fileName()
								  + ":<br/>"
								  + db.lastError().text()
								  + tr("<br/>It is probably not a database."));
		db.close();
		return;
	}
	else
	{
		isOpened = true;

		// check for sqlite library version
		QString ver;
		if (q.exec("select sqlite_version(*);"))
		{
			if(!q.lastError().isValid())
			{
				q.next();
				ver = q.value(0).toString();
			}
			else
				ver = "n/a";
		}
		else
			ver = "n/a";
		m_sqliteVersionLabel->setText("Sqlite: " + ver);

#ifdef ENABLE_EXTENSIONS
		// load startup exceptions
		bool loadE = Preferences::instance()->allowExtensionLoading();
		handleExtensions(loadE);
		if (loadE && Preferences::instance()->extensionList().count() != 0)
		{
			schemaBrowser->appendExtensions(
					Database::loadExtension(Preferences::instance()->extensionList())
					);
			statusBar()->showMessage(tr("Startup extensions loaded successfully"));
		}
#endif

		attachedDb.clear();
		attachedDb["main"] = SESSION_NAME;
	
		QFileInfo fi(fileName);
		QDir::setCurrent(fi.absolutePath());
		m_mainDbPath = QDir::toNativeSeparators(QDir::currentPath() + "/" + fi.fileName());
	
		updateRecent(fileName);
	
		// Build tree & clean model
		schemaBrowser->tableTree->buildTree();
		schemaBrowser->buildPragmasTree();
		dataViewer->setTableModel(new QSqlQueryModel(), false);
		m_activeItem = 0;
	
		// Update the title
		setWindowTitle(fi.fileName() + " - " + m_appName);
	}

	// Enable UI
	schemaBrowser->setEnabled(isOpened);
	databaseMenu->setEnabled(isOpened);
	adminMenu->setEnabled(isOpened);
	sqlEditor->setEnabled(isOpened);
	dataViewer->setEnabled(isOpened);
}

#ifdef ENABLE_EXTENSIONS
void LiteManWindow::handleExtensions(bool enable)
{
	QString e(enable ? tr("enabled") : tr("disabled"));
	loadExtensionAct->setEnabled(enable);
	if (Database::setEnableExtensions(enable))
		statusBar()->showMessage(tr("Extensions loading is %1").arg(e));
}
#endif

void LiteManWindow::openRecent()
{
	QAction *action = qobject_cast<QAction *>(sender());
	if (action)
		open(action->data().toString());
}

void LiteManWindow::about()
{
	QMessageBox::about(this, tr("About"),
					   tr("Sqliteman - SQLite databases made easy\n\n")
					   + tr("Version ")
					   + SQLITEMAN_VERSION + "\n"
					   + tr("Parts")
					   + "(c) 2007 Petr Vanek\n"
                       + tr("Parts")
					   + "(c) 2015 Richard Parkins\n"
					   + buildtime);
}

void LiteManWindow::aboutQt()
{
	QMessageBox::aboutQt(this, m_appName);
}

void LiteManWindow::help()
{
	if (!helpBrowser)
		helpBrowser = new HelpBrowser(m_lang, this);
	helpBrowser->show();
}

void LiteManWindow::buildQuery()
{
	QueryEditorDialog dlg(this);

	int ret = dlg.exec();

	if(ret == QDialog::Accepted)
	{
		/*runQuery*/
		execSql(dlg.statement());
	}
}

void LiteManWindow::contextBuildQuery()
{
	QTreeWidgetItem * item = schemaBrowser->tableTree->currentItem();
	if (!item) { return; }
	
	QueryEditorDialog dlg(item, this);

	int ret = dlg.exec();

	if(ret == QDialog::Accepted)
	{
		/*runQuery*/
		execSql(dlg.statement());
	}
}

void LiteManWindow::handleSqlEditor()
{
	sqlEditor->setVisible(!sqlEditor->isVisible());
	execSqlAct->setChecked(sqlEditor->isVisible());
}

void LiteManWindow::handleObjectBrowser()
{
	schemaBrowser->setVisible(!schemaBrowser->isVisible());
	objectBrowserAct->setChecked(schemaBrowser->isVisible());
}

void LiteManWindow::handleDataViewer()
{
	dataViewer->setVisible(!dataViewer->isVisible());
	dataViewerAct->setChecked(dataViewer->isVisible());
}

void LiteManWindow::execSql(QString query)
{
	if (query.isEmpty() || query.isNull())
	{
		QMessageBox::warning(this, tr("No SQL statement"), tr("You are trying to run an undefined SQL query. Hint: select your query in the editor"));
		return;
	}
	if (!checkForPending()) { return; }

	dataViewer->freeResources(dataViewer->tableData());
	m_activeItem = 0;
	sqlEditor->setStatusMessage();

	QTime time;
	time.start();

	// Run query
	SqlQueryModel * model = new SqlQueryModel(this);
	model->setQuery(query, QSqlDatabase::database(SESSION_NAME));

	if (!dataViewer->setTableModel(model, false))
		return;

	sqlEditor->setStatusMessage(tr("Duration: %1 seconds").arg(time.elapsed() / 1000.0));
	
	// Check For Error in the SQL
	if(model->lastError().isValid())
	{
		dataViewer->setStatusText(tr("Query Error: ")
								  + model->lastError().text()
								  + "<br/>"
								  + tr("using sql statement:")
								  + "<br/>"
								  + query);
	}
	else
	{
		QString cached;
		if ((model->rowCount() != 0) && (model->canFetchMore()))
			cached = DataViewer::canFetchMore() + "<br/>";
		else
			cached = "";
		dataViewer->setStatusText(tr("Query OK:")
								  + "<br/>"
								  + query
								  + "<br/>"
								  + tr("Row(s) returned: ")
								  + QString("%1").arg(model->rowCount())
								  + cached);
		if (Utils::updateObjectTree(query))
		{
			schemaBrowser->tableTree->buildTree();
		}
	}
}

void LiteManWindow::exportSchema()
{
	QString fileName = QFileDialog::getSaveFileName(this, tr("Export Schema"),
                                                    QDir::currentPath(),
                                                    tr("SQL File (*.sql)"));

	if (fileName.isNull())
		return;

	Database::exportSql(fileName);
}

void LiteManWindow::dumpDatabase()
{
	QString fileName = QFileDialog::getSaveFileName(this, tr("Export Database"),
                                                    QDir::currentPath(),
                                                    tr("SQL File (*.sql)"));

	if (fileName.isNull())
		return;

	if (Database::dumpDatabase(fileName))
	{
		QMessageBox::information(this, m_appName,
								 tr("Dump written into: %1").arg(fileName));
	}
}

void LiteManWindow::createTable()
{
	CreateTableDialog dlg(this);
	dlg.exec();
	if (dlg.updated)
	{
		foreach (QTreeWidgetItem* item,
			schemaBrowser->tableTree->searchMask(
				schemaBrowser->tableTree->trTables))
		{
			if (item->type() == TableTree::TablesItemType)
				schemaBrowser->tableTree->buildTables(item, item->text(1));
		}
	}
}

void LiteManWindow::alterTable()
{
	QTreeWidgetItem * item = schemaBrowser->tableTree->currentItem();

	if (!item) { return; }

	bool isActive = m_activeItem == item;
	dataViewer->saveSelection();
	AlterTableDialog dlg(this, item, isActive);
	dlg.exec();
	if (dlg.updateStage == 2)
	{
		schemaBrowser->tableTree->buildTableItem(item, true);
		if (isActive)
		{
			m_activeItem = 0; // we've changed it
			treeItemActivated(item, 0);
			dataViewer->reSelect();
		}
	}
}

void LiteManWindow::renameTable()
{
	QTreeWidgetItem * item = schemaBrowser->tableTree->currentItem();
	if (!item) { return; }

	QString text = item->text(0);
	while (1)
	{
		bool ok = true;
		text = QInputDialog::getText(this, m_appName,
									 tr("New table name:"),
									 QLineEdit::Normal, text, &ok);
		if (text.isEmpty() || !ok) { return; }
		
		if (text.contains(QRegExp
			("\\s|-|\\]|\\[|[`!\"%&*()+={}:;@'~#|\\\\<,>.?/^]")))
		{
			int ret = QMessageBox::question(this, m_appName,
				tr("A table named ")
				+ text
				+ tr(" will not display correctly. "
					 "Are you sure you want to rename it?\n")
				+ tr("\nYes to rename, Cancel to try another name."),
				QMessageBox::Yes, QMessageBox::Cancel);
			if (ret == QMessageBox::Cancel) { continue; }
		}
		if (text == item->text(0)) { return; }
		bool isActive = m_activeItem == item;
		dataViewer->saveSelection();
		// check needed because QSqlTableModel holds the table name
		if ((!isActive) || (checkForPending()))
		{
			QString sql = QString("ALTER TABLE")
						  + Utils::quote(item->text(1))
						  + "."
						  + Utils::quote(item->text(0))
						  + " RENAME TO "
						  + Utils::quote(text)
						  + ";";
			QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
			if (query.lastError().isValid())
			{
				dataViewer->setStatusText(tr("Cannot rename table ")
										  + item->text(1)
										  + tr(".")
										  + item->text(0)
										  + ":<br/>"
										  + query.lastError().text()
										  + tr("<br/>using sql statement:<br/>")
										  + sql);
			}
			else
			{
				item->setText(0, text);
				if (isActive)
				{
					m_activeItem = 0; // we've changed it
					treeItemActivated(item, 0);
					dataViewer->reSelect();
				}
			}
		}
		return;
	}
}

void LiteManWindow::populateTable()
{
	QTreeWidgetItem * item = schemaBrowser->tableTree->currentItem();
	if (!item) { return; }
	
	bool isActive = m_activeItem == item;
	if (isActive && !checkForPending()) { return; }
	PopulatorDialog dlg(this, item->text(0), item->text(1));
	dlg.exec();
	if (isActive && dlg.updated) {
		dataViewer->saveSelection();
		m_activeItem = 0; // we've changed it
		treeItemActivated(item, 0);
		dataViewer->reSelect();
	}
}

void LiteManWindow::importTable()
{
	//FIXME no popup seen if ~isActive
	QTreeWidgetItem * item = schemaBrowser->tableTree->currentItem();

	bool isActive = m_activeItem == item;
	if (item && isActive && !checkForPending()) { return; }
	ImportTableDialog dlg(this, item ? item->text(0) : "",
							    item ? item->text(1) : "main");
	if (isActive && (dlg.exec() == QDialog::Accepted))
	{
		dataViewer->saveSelection();
		m_activeItem = 0; // we've changed it
		treeItemActivated(item, 0);
		dataViewer->reSelect();
	}
}

void LiteManWindow::dropTable()
{
	QTreeWidgetItem * item = schemaBrowser->tableTree->currentItem();

	if(!item)
		return;

	bool isActive = m_activeItem == item;

	int ret = QMessageBox::question(this, m_appName,
					tr("Are you sure that you wish to drop the table \"%1\"?")
					.arg(item->text(0)),
					QMessageBox::Yes, QMessageBox::No);

	if(ret == QMessageBox::Yes)
	{
		// don't check for pending, we're dropping it anyway
		QString sql = QString("DROP TABLE")
					  + Utils::quote(item->text(1))
					  + "."
					  + Utils::quote(item->text(0))
					  + ";";
		QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
		if (query.lastError().isValid())
		{
			dataViewer->setStatusText(tr("Cannot drop table ")
									  + item->text(1)
									  + tr(".")
									  + item->text(0)
									  + ":<br/>"
									  + query.lastError().text()
									  + tr("<br/>using sql statement:<br/>")
									  + sql);
		}
		else
		{
			schemaBrowser->tableTree->buildTables(item->parent(),
												  item->text(1));
			if (isActive)
			{
				dataViewer->setNotPending();
				dataViewer->setTableModel(new QSqlQueryModel(), false);
				m_activeItem = 0;
			}
		}
	}
}

void LiteManWindow::createView()
{
	CreateViewDialog dia("", "", this);

	dia.exec();
	if(dia.update)
		schemaBrowser->tableTree->buildViewTree(dia.schema(), dia.name());
}

void LiteManWindow::alterView()
{
	//FIXME allow Alter View to change name like Alter Table
	QTreeWidgetItem * item = schemaBrowser->tableTree->currentItem();
	bool isActive = m_activeItem == item;
	// Can't have pending update on a view, so no checkForPending() here
	// This might change if we allow editing on views with triggers
	AlterViewDialog dia(item->text(0), item->text(1), this);
	dia.exec();
	if (dia.update)
	{
		QTreeWidgetItem * triggers = item->child(0);
		if (triggers)
		{
			schemaBrowser->tableTree->buildTriggers(triggers, item->text(1),
													item->text(0));
		}
		if (isActive)
		{
			dataViewer->saveSelection();
			m_activeItem = 0; // we've changed it
			treeItemActivated(item, 0);
			dataViewer->reSelect();
		}
	}
}

void LiteManWindow::dropView()
{
	QTreeWidgetItem * item = schemaBrowser->tableTree->currentItem();

	if(!item)
		return;
	bool isActive = m_activeItem == item;
	// Can't have pending update on a view, so no checkForPending() here
	// Ask the user for confirmation
	int ret = QMessageBox::question(this, m_appName,
					tr("Are you sure that you wish to drop the view \"%1\"?")
					.arg(item->text(0)),
					QMessageBox::Yes, QMessageBox::No);

	if(ret == QMessageBox::Yes)
	{
		QString sql = QString("DROP VIEW")
					  + Utils::quote(item->text(1))
					  + "."
					  + Utils::quote(item->text(0))
					  + ";";
		QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
		if (query.lastError().isValid())
		{
			dataViewer->setStatusText(tr("Cannot drop view ")
									  + item->text(1)
									  + tr(".")
									  + item->text(0)
									  + ":<br/>"
									  + query.lastError().text()
									  + tr("<br/>using sql statement:<br/>")
									  + sql);
		}
		else
		{
			schemaBrowser->tableTree->buildViews(item->parent(), item->text(1));
			if (isActive)
			{
				dataViewer->setTableModel(new QSqlQueryModel(), false);
				m_activeItem = 0;
			}
		}
	}
}

void LiteManWindow::createIndex()
{
	QString table(schemaBrowser->tableTree->currentItem()->parent()->text(0));
	QString schema(schemaBrowser->tableTree->currentItem()->parent()->text(1));
	CreateIndexDialog dia(table, schema, this);
	dia.exec();
	if (dia.update)
		schemaBrowser->tableTree->buildIndexes(schemaBrowser->tableTree->currentItem(), schema, table);
}

void LiteManWindow::dropIndex()
{
	QTreeWidgetItem * item = schemaBrowser->tableTree->currentItem();

	if(!item)
		return;

	// Ask the user for confirmation
	int ret = QMessageBox::question(this, m_appName,
					tr("Are you sure that you wish to drop the index \"%1\"?")
					.arg(item->text(0)),
					QMessageBox::Yes, QMessageBox::No);

	if(ret == QMessageBox::Yes)
	{
		QString sql = QString("DROP INDEX")
					  + Utils::quote(item->text(1))
					  + "."
					  + Utils::quote(item->text(0))
					  + ";";
		QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
		if (query.lastError().isValid())
		{
			dataViewer->setStatusText(tr("Cannot drop index ")
									  + item->text(1)
									  + tr(".")
									  + item->text(0)
									  + ":<br/>"
									  + query.lastError().text()
									  + tr("<br/>using sql statement:<br/>")
									  + sql);
		}
		else
		{
			schemaBrowser->tableTree->buildIndexes(
				item->parent(), item->text(1), item->parent()->parent()->text(0));
		}
	}
}

void LiteManWindow::treeItemActivated(QTreeWidgetItem * item, int /*column*/)
{
	if (   (!item)
		|| (m_activeItem == item)
		|| !checkForPending())
	{
		return;
	}

	if (item->type() == TableTree::TableType || item->type() == TableTree::ViewType
		|| item->type() == TableTree::SystemType)
	{
		dataViewer->freeResources(dataViewer->tableData());
		if (item->type() == TableTree::ViewType || item->type() == TableTree::SystemType)
		{
			SqlQueryModel * model = new SqlQueryModel(0);
			model->setQuery(QString("select * from ")
							+ Utils::quote(item->text(1))
							+ "."
							+ Utils::quote(item->text(0)),
							QSqlDatabase::database(SESSION_NAME));
			dataViewer->setTableModel(model, false);
		}
		else
		{
			SqlTableModel * model = new SqlTableModel(0, QSqlDatabase::database(attachedDb[item->text(1)]));
			model->setSchema(item->text(1));
			model->setTable(item->text(0));
			model->select();
			model->setEditStrategy(SqlTableModel::OnManualSubmit);
			dataViewer->setTableModel(model, true);
		}
		m_activeItem = item;
	}
}

void LiteManWindow::tableTree_currentItemChanged(QTreeWidgetItem* cur, QTreeWidgetItem* /*prev*/)
{
	contextMenu->clear();
	if (!cur)
		return;

	switch(cur->type())
	{
		case TableTree::TablesItemType:
			contextMenu->addAction(createTableAct);
			break;

		case TableTree::ViewsItemType:
			contextMenu->addAction(createViewAct);
			break;

		case TableTree::TableType:
			contextMenu->addAction(describeTableAct);
			contextMenu->addAction(alterTableAct);
			contextMenu->addAction(renameTableAct);
			contextMenu->addAction(dropTableAct);
			contextMenu->addAction(contextBuildQueryAct);
			contextMenu->addAction(reindexAct);
			contextMenu->addSeparator();
			contextMenu->addAction(importTableAct);
			contextMenu->addAction(populateTableAct);
			break;

		case TableTree::ViewType:
			contextMenu->addAction(describeViewAct);
			contextMenu->addAction(alterViewAct);
			contextMenu->addAction(dropViewAct);
			break;

		case TableTree::IndexType:
			contextMenu->addAction(describeIndexAct);
			contextMenu->addAction(dropIndexAct);
			contextMenu->addAction(reindexAct);
			break;

		case TableTree::IndexesItemType:
			contextMenu->addAction(createIndexAct);
			break;

		case TableTree::DatabaseItemType:
			contextMenu->addAction(refreshTreeAct);
			if (cur->text(0) != "main")
				contextMenu->addAction(detachAct);
			break;

		case TableTree::TriggersItemType:
			contextMenu->addAction(createTriggerAct);
			if (cur->parent()->type() != TableTree::ViewType)
				contextMenu->addAction(consTriggAct);
			break;

		case TableTree::TriggerType:
			contextMenu->addAction(describeTriggerAct);
			contextMenu->addAction(alterTriggerAct);
			contextMenu->addAction(dropTriggerAct);
			break;
	}
	contextMenu->setDisabled(contextMenu->actions().count() == 0);
}


void LiteManWindow::treeContextMenuOpened(const QPoint & pos)
{
	if (contextMenu->actions().count() != 0)
		contextMenu->exec(schemaBrowser->tableTree->viewport()->mapToGlobal(pos));
}

void LiteManWindow::describeObject()
{
	QTreeWidgetItem * item = schemaBrowser->tableTree->currentItem();
	/*runQuery*/
// 	execSql(QString("select sql as \"%1\" from \"%2\".sqlite_master where name = '%3';")
// 			.arg(tr("Describe %1").arg(item->text(0).toUpper()))
// 			.arg(item->text(1))
// 			.arg(item->text(0)));
	QString desc(Database::describeObject(item->text(0), item->text(1)));
	dataViewer->sqlScriptStart();
	dataViewer->showSqlScriptResult("-- " + tr("Describe %1").arg(item->text(0).toUpper()));
	dataViewer->showSqlScriptResult(desc);
}

void LiteManWindow::analyzeDialog()
{
	AnalyzeDialog *dia = new AnalyzeDialog(this);
	dia->exec();
	delete dia;
	foreach (QTreeWidgetItem* item, schemaBrowser->tableTree->searchMask(schemaBrowser->tableTree->trSys))
	{
		if (item->type() == TableTree::SystemItemType)
			schemaBrowser->tableTree->buildCatalogue(item, item->text(1));
	}
}

void LiteManWindow::vacuumDialog()
{
	VacuumDialog *dia = new VacuumDialog(this);
	dia->exec();
	delete dia;
}

void LiteManWindow::attachDatabase()
{
	QString fileName;
	fileName = QFileDialog::getOpenFileName(this, tr("Attach Database"), QDir::currentPath(), tr("SQLite database (*)"));
	if (fileName.isEmpty())
		return;
	bool ok;
	QFileInfo f(fileName);
	QString schema = QInputDialog::getText(this, tr("Attach Database"),
										   tr("Enter a Schema Alias:"),
										   QLineEdit::Normal,
										   f.baseName(), &ok);
	if (!ok || schema.isEmpty())
		return;
	QString sql = QString("ATTACH DATABASE ")
				  + Utils::literal(fileName)
				  + " as "
				  + Utils::quote(schema)
				  + ";";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	if (query.lastError().isValid())
	{
		dataViewer->setStatusText(tr("Cannot attach database ")
								  + fileName
								  + ":<br/>"
								  + query.lastError().text()
								  + tr("<br/>using sql statement:<br/>")
								  + sql);
	}
	else
	{
		attachedDb[schema] = Database::sessionName(schema);
		QSqlDatabase db =
			QSqlDatabase::addDatabase("QSQLITE", attachedDb[schema]);
		db.setDatabaseName(fileName);
		if (!db.open())
		{
			dataViewer->setStatusText(tr("Cannot open or create ")
									  + QFileInfo(fileName).fileName()
									  + ":<br/>"
									  + db.lastError().text());
			QSqlQuery qundo(QString("DETACH DATABASE ")
							+ Utils::quote(schema)
							+ ";",
							db);
			attachedDb.remove(schema);
		}
		else
		{
			QSqlQuery q("select 1 from sqlite_master where 1=2", db);
			if (!q.exec())
			{
				dataViewer->setStatusText(
					tr("Cannot access ")
					+ QFileInfo(fileName).fileName()
					+ ":<br/>"
					+ db.lastError().text()
					+ tr("<br/>It is probably not a database."));
				QSqlQuery qundo(QString("DETACH DATABASE ")
								+ Utils::quote(schema)
								+ ";",
								db);
				db.close();
				attachedDb.remove(schema);
			}
			else
			{
				schemaBrowser->tableTree->buildDatabase(schema);
			}
		}
	}
}

void LiteManWindow::detachDatabase()
{
	QString dbname(schemaBrowser->tableTree->currentItem()->text(0));
	QString sql = QString("DETACH DATABASE ")
				  + Utils::quote(dbname)
				  + ";";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	if (query.lastError().isValid())
	{
		dataViewer->setStatusText(tr("Cannot detach database ")
								  + dbname
								  + ":<br/>"
								  + query.lastError().text()
								  + tr("<br/>using sql statement:<br/>")
								  + sql);
	}
	else
	{
		QSqlDatabase::database(attachedDb[dbname]).rollback();
		QSqlDatabase::database(attachedDb[dbname]).close();
		attachedDb.remove(dbname);
		delete schemaBrowser->tableTree->currentItem();
	}
}

void LiteManWindow::loadExtension()
{
	QString mask(tr("Sqlite3 extensions "));
#ifdef Q_WS_WIN
	mask += "(*.dll)";
#else
	mask += "(*.so)";
#endif

	QStringList files = QFileDialog::getOpenFileNames(
						this,
						tr("Select one or more Sqlite3 extensions to load"),
						QDir::currentPath(),
						mask);
	if (files.count() == 0)
		return;

	schemaBrowser->appendExtensions(Database::loadExtension(files), true);
}

void LiteManWindow::createTrigger()
{
	QTreeWidgetItem * item = schemaBrowser->tableTree->currentItem();
	QString table(item->parent()->text(0));
	QString schema(item->parent()->text(1));
	CreateTriggerDialog *dia = new CreateTriggerDialog(table, schema,
											item->parent()->type(), this);
	dia->exec();
	if (dia->update)
		schemaBrowser->tableTree->buildTriggers(item, schema, table);
	delete dia;
}

void LiteManWindow::alterTrigger()
{
	QString table(schemaBrowser->tableTree->currentItem()->text(0));
	QString schema(schemaBrowser->tableTree->currentItem()->text(1));
	AlterTriggerDialog *dia = new AlterTriggerDialog(table, schema, this);
	dia->exec();
	delete dia;
}


void LiteManWindow::dropTrigger()
{
	QTreeWidgetItem * item = schemaBrowser->tableTree->currentItem();

	if(!item)
		return;

	// Ask the user for confirmation
	int ret = QMessageBox::question(this, m_appName,
					tr("Are you sure that you wish to drop the trigger \"%1\"?")
					.arg(item->text(0)),
					QMessageBox::Yes, QMessageBox::No);

	if(ret == QMessageBox::Yes)
	{
		QString sql = QString("DROP TRIGGER ")
					  + Utils::quote(item->text(1))
					  + "."
					  + Utils::quote(item->text(0))
					  + ";";
		QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
		if (query.lastError().isValid())
		{
			dataViewer->setStatusText(tr("Cannot drop trigger ")
									  + item->text(1)
									  + tr(".")
									  + item->text(0)
									  + ":<br/>"
									  + query.lastError().text()
									  + tr("<br/>using sql statement:<br/>")
									  + sql);
		}
		else
		{
			schemaBrowser->tableTree->buildTriggers(
				item->parent(), item->text(1),
				item->parent()->parent()->text(0));
		}
	}
}

void LiteManWindow::constraintTriggers()
{
	QString table(schemaBrowser->tableTree->currentItem()->parent()->text(0));
	QString schema(schemaBrowser->tableTree->currentItem()->parent()->text(1));
	ConstraintsDialog dia(table, schema, this);
	dia.exec();
	if (dia.update)
		schemaBrowser->tableTree->buildTriggers(schemaBrowser->tableTree->currentItem(), schema, table);
}

void LiteManWindow::reindex()
{
	QTreeWidgetItem * item = schemaBrowser->tableTree->currentItem();
	if (item)
	{
		QString sql(QString("REINDEX ")
					+ Utils::quote(item->text(1))
					+ "."
					+ Utils::quote(item->text(0))
					+ ";");
		QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
		if (query.lastError().isValid())
		{
			dataViewer->setStatusText(tr("Cannot reindex ")
									  + item->text(1)
									  + tr(".")
									  + item->text(0)
									  + ":<br/>"
									  + query.lastError().text()
									  + tr("<br/>using sql statement:<br/>")
									  + sql);
		}
	}
}

void LiteManWindow::refreshTable()
{
	/* SQL code in the SQL editor may have modified or even removed the current
	 * table.
	 */
	dataViewer->setTableModel(new QSqlQueryModel(), false);
	m_activeItem = 0;
}

void LiteManWindow::preferences()
{
	PreferencesDialog prefs(this);
	if (prefs.exec())
		if (prefs.saveSettings())
		{
			emit prefsChanged();
#ifdef ENABLE_EXTENSIONS
			handleExtensions(Preferences::instance()->allowExtensionLoading());
#endif
		}
}
