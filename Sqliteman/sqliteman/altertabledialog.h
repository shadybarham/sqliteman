/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef ALTERTABLEDIALOG_H
#define ALTERTABLEDIALOG_H

#include "database.h"
#include "litemanwindow.h"
#include "tableeditordialog.h"

class QTreeWidgetItem;

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
		/*! \brief Mappings of the columns to their indexes
		E.g. "id" -- "mytab_ix1" etc.
		It's used in the simulated DROP COLUMN checking */
		QMap<QString,QStringList> m_columnIndexMap;

		QTreeWidgetItem * m_item;
		int m_protectedRows;

		//! \brief Indicate how much protected colums are marked for DROPping.
		int m_dropColumns;

		//! \brief Fill the GUI with table structure.
		void resetStructure();
		//! \brief Setup the OK button if there is something changed
		void checkChanges();

		/*! \brief Analyze user changes and performs the ALTER TABLE ADD COLUM sqls
		\retval bool true on succes, false on error
		 */
		bool addColumns();

		/*! \brief Perform a rename table action if it's required.
		This is done if user edits table name widget.
		\retval true on success.
		*/
		bool renameTable();

		/*! \brief Execute statement, handle its errors and outputs message to the GUI.
		\param statement a SQL statement as QString
		\param message a text message to display in the log widget
		\param tmpName an addon text for log in the case of error
		\retval bool true on SQL succes
		*/
		bool execSql(const QString & statement, const QString & message, const QString & tmpName=0);

		//! \brief Returns a list of DDL statements to recreate reqired obejcts after all.
		QStringList originalSource();

		// We ought to be able use use parent() for this, but for some reason
		// qobject_cast<LiteManWindow*>(parent()) doesn't work
		LiteManWindow * creator;

		// true if altering currently active table
		bool m_alteringActive;

	private slots:
		void addField();
		void removeField();
		void fieldSelected();
		//! \brief Check if to allow user changes in the table column (qtable row).
		void cellClicked(int, int);

		void dropItem_stateChanged(int);

		void createButton_clicked();
};

#endif
