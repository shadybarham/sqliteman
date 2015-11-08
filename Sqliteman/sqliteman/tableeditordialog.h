/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef TABLEEDITORDIALOG_H
#define TABLEEDITORDIALOG_H

#include <QDialog>

#include "database.h"
#include "ui_tableeditordialog.h"
#include "preferences.h"

class LiteManWindow;

/*! \brief A base dialog for creating and editing tables.
This dialog is taken as a inheritance parent for AlterTableDialog
and CreateTableDialog and CreatViewDialog.
\author Petr Vanek <petr@scribus.info>
\author Igor Khanin
*/
class TableEditorDialog : public QDialog
{
		Q_OBJECT
	public:
		TableEditorDialog(LiteManWindow * parent);
		~TableEditorDialog();

		Ui::TableEditorDialog ui;

		void resultAppend(QString text);

		void addField(QString oldName, QString oldType,
					  int x, QString oldDefault);

		bool checkOk(QString newName);
		QString schema();
		QString createdName();

		Preferences * m_prefs;
		bool updated;

	protected:
		virtual int oldColumnNumber(int i) = 0;
		virtual bool checkRetained(int i) = 0;
		virtual bool checkColumn(int i, QString cname,
								 QString type, QString cextra) = 0;
		QString getSQLfromDesign();
		QString getSQLfromGUI();
		QString getFullName();
		void setFirstLine();
		void setDirty();
		QString m_tableOrView;
		int m_tabWidgetIndex;
		bool m_dirty; // SQL has been edited
		bool m_dubious; // some column has an empty name
		QWidget * m_oldWidget; // widget from which we got SQL

		// We ought to be able use use parent() for this, but for some reason
		// qobject_cast<LiteManWindow*>(parent()) doesn't work
		LiteManWindow * creator;

	public slots:
		virtual void addField();
		virtual void removeField();
		virtual void fieldSelected();
		void tableNameChanged();
		void tabWidget_currentChanged(int index);
		virtual void checkChanges() = 0;
};

#endif
