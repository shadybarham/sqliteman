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

/*! \brief A base dialog for creating and editing tables.
This dialog is taken as a inheritance parent for AlterTableDialog
and CreateTableDialog.
\author Petr Vanek <petr@scribus.info>
\author Igor Khanin
*/
class TableEditorDialog : public QDialog
{
		Q_OBJECT
	public:
		TableEditorDialog(QWidget * parent);
		~TableEditorDialog();

		Ui::TableEditorDialog ui;

		void resultAppend(QString text);

		void addField(QString oldName, QString oldType,
					  int x, QString oldDefault);

		bool checkOk(QString newName);

		Preferences * m_prefs;

	protected:
		virtual void checkChanges() = 0;
		virtual bool checkRetained(int i) = 0;
		virtual bool checkColumn(int i, QString cname,
								 QString type, QString cextra) = 0;
		QString getSQLfromGUI();
		QString getFullName();

	public slots:
		virtual void addField();
		virtual void removeField();
		virtual void fieldSelected();
};

#endif
