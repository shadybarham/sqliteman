/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef ALTERTABLEDIALOG_H
#define ALTERTABLEDIALOG_H

#include "database.h"
#include "tableeditordialog.h"

class QTreeWidgetItem;
class QPushButton;

/*! \brief Handle alter table features.
Sqlite3 has very limited ALTER TABLE feature set, so
there is only support for "table rename" and "add column".
Table Renaming is handled directly in the LiteManWindow
method renameTable().
Adding columns - see addColumns(). It's a wrapper around
plain ALTER TABLE ADD COLUMN statement.
Drop Column is using a workaround with tmp table, insert-select
statement and renaming. See createButton_clicked()
\author Petr Vanek <petr@scribus.info>
*/

class AlterTableDialog : public TableEditorDialog
{
	Q_OBJECT

	public:
		AlterTableDialog(LiteManWindow * parent = 0,
						 QTreeWidgetItem * item = 0,
						 const bool isActive = false
						);
		~AlterTableDialog(){};

		// These ought to be enums, but they're so clumsy in C++
		// 0 nothing yet
		// 1 table has been renamed
		// 2 table has been modified
		int updateStage;

	private:
		QTreeWidgetItem * m_item;
		QPushButton * m_alterButton;
		
		QList<FieldInfo> m_fields;
		bool m_hadRowid;

		//! \brief Fill the GUI with table structure.
		void resetStructure();

		/*! \brief Perform a rename table action if it's required.
		This is done if user edits table name widget.
		\retval true on success.
		*/
		bool renameTable(QString oldTableName, QString newTableName);

		/*! \brief Execute statement, handle its errors and outputs message to the GUI.
		\param statement a SQL statement as QString
		\param message a text message to display in the log widget
		\retval bool true on SQL succes
		*/
		bool execSql(const QString & statement, const QString & message);
		bool doRollback(QString message);

		bool checkRetained(int i);
		bool checkColumn(int i, QString cname,
						 QString ctype, QString cextra);

		//! \brief Returns a list of DDL statements to recreate reqired obejcts after all.
		QStringList originalSource(QString tableName);

		// true if altering currently active table
		bool m_alteringActive;

		// something changed other than the name
		bool m_altered;

		// list of column names not dropped
		QList<int> m_keptColumns;

	private slots:
		void addField();
		void removeField();
		void fieldSelected();
		//! \brief Check if to allow user changes in the table column (qtable row).
		void cellClicked(int, int);

		void alterButton_clicked();

		//! \brief Setup the Alter button if there is something changed
		void checkChanges();
};

#endif
