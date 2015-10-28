/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef CREATEVIEWDIALOG_H
#define CREATEVIEWDIALOG_H

#include <qwidget.h>

#include "litemanwindow.h"
#include "tableeditordialog.h"

class QTreeWidgetItem;
class QPushButton;

/*! \brief GUI for view creation
\author Petr Vanek <petr@scribus.info>
*/
class CreateViewDialog : public TableEditorDialog
{
	Q_OBJECT

	public:
		CreateViewDialog(LiteManWindow * parent = 0,
						  QTreeWidgetItem * item = 0);
		~CreateViewDialog(){};
		void setText(QString query);

		bool updated;

	private:

		QString getSQLfromGUI();
		bool checkRetained(int i);
		bool checkColumn(int i, QString cname,
						 QString ctype, QString cextra);

		bool m_dirty; // SQL has been edited

		// We ought to be able use use parent() for this, but for some reason
		// qobject_cast<LiteManWindow*>(parent()) doesn't work
		LiteManWindow * creator;
		QPushButton * m_createButton;
		int m_tabWidgetIndex;
		
	signals:
		/*! \brief Rebuild part of the tree */
		void rebuildViewTree(QString schema, QString name);

	private slots:
		void createButton_clicked();
		void tabWidget_currentChanged(int index);
		void checkChanges();
		void setDirty();
};

#endif
