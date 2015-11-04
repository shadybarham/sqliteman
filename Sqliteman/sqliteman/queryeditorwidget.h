/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef QUERYEDITORWIDGET_H
#define QUERYEDITORWIDGET_H

class QTreeWidgetItem;
class QueryStringModel;

#include "database.h"
#include "ui_queryeditorwidget.h"

/*!
 * @brief A widget for creating and editing queries
 * \author Igor Khanin
 * \author Petr Vanek <petr@scribus.info>
 * \author Richard Parkins extracted from QueryEditorDialog
 */
class QueryEditorWidget : public QWidget, public Ui::QueryEditorWidget
{
	Q_OBJECT

	public:
		/*!
		 * @brief Creates the query editor.
		 * @param parent The parent widget.
		 */
		QueryEditorWidget(QWidget * parent = 0);
		void setItem(QTreeWidgetItem * item);
		//! \brief generates a valid SQL statement using the values in the dialog
		QString statement();
		void treeChanged();
		void tableAltered(QString oldName, QTreeWidgetItem * item);

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

#endif //QUERYEDITORWIDGET_H
