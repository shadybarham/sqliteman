/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef CREATETABLEDIALOG_H
#define CREATETABLEDIALOG_H

#include "litemanwindow.h"
#include "tableeditordialog.h"

class QTreeWidgetItem;
class QPushButton;

/*! \brief A GUI for CREATE TABLE procedure.
\author Petr Vanek <petr@scribus.info>
*/
class CreateTableDialog : public TableEditorDialog
{
	Q_OBJECT

	public:
		CreateTableDialog(LiteManWindow * parent = 0,
						  QTreeWidgetItem * item = 0);
		~CreateTableDialog(){};

		bool updated;

	private:
		/*! \brief Analyze user changes and performs the CREATE TABLE sql
		*/
		QString getSQLfromGUI();

		bool m_dirty; // SQL has been edited

		// We ought to be able use use parent() for this, but for some reason
		// qobject_cast<LiteManWindow*>(parent()) doesn't work
		LiteManWindow * creator;
		QPushButton * m_createButton;
		int m_tabWidgetIndex;

	private slots:
		void createButton_clicked();
		void tabWidget_currentChanged(int index);
		void checkChanges();
		void setDirty();
};

#endif
