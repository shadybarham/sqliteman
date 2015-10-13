/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef QUERYEDITORDIALOG_H
#define QUERYEDITORDIALOG_H

class QTreeWidgetItem;
class QueryStringModel;

#include "database.h"
#include "ui_queryeditordialog.h"

/*!
 * @brief A dialog for creating and editing queries
 * \author Igor Khanin
 * \author Petr Vanek <petr@scribus.info>
 */
class QueryEditorDialog : public QDialog, public Ui::QueryEditorDialog
{
	Q_OBJECT

	public:
		/*!
		 * @brief Creates the query editor.
		 * @param parent The parent widget for the dialog.
		 */
		QueryEditorDialog(QWidget * parent = 0);
		~QueryEditorDialog();
		void setItem(QTreeWidgetItem * item);
		//! \brief generates a valid SQL statement using the values in the dialog
		QString statement();
		void treeChanged();
		void tableAltered(QString oldName, QString newName);

	private:
		bool initialised;
		QString m_schema;
		QString m_table;
		QueryStringModel * columnModel;
		QueryStringModel * selectModel;
		QString m_rowid;
		QStringList m_columnList;

		void resetModel();
		QStringList getColumns();
		void resetTableList();
		void resetSchemaList();

private slots:
		void tableSelected(const QString & table);
		void schemaSelected(const QString & schema);
		// Term tab
		void moreTerms();
		void lessTerms();
		// Fields tab
		void addAllSelect();
		void addSelect();
		void removeAllSelect();
		void removeSelect();
		// Order by tab
		void moreOrders();
		void lessOrders();
		void relationsIndexChanged(const QString &);
		void resetClicked();
};

#endif //QUERYEDITORDIALOG_H
