/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef IMPORTTABLEDIALOG_H
#define IMPORTTABLEDIALOG_H

#include "litemanwindow.h"
#include "sqlparser.h"
#include "ui_importtabledialog.h"

class QTreeWidgetItem;



/*! \brief Import data into table using various importer types.
\note XML import requires Qt library at least in the 4.3.0 version.
\author Petr Vanek <petr@scribus.info>
*/
class ImportTableDialog : public QDialog, public Ui::ImportTableDialog
{
	Q_OBJECT

	public:
		ImportTableDialog(LiteManWindow * parent = 0,
						  const QString & tableName = 0,
						  const QString & schema = 0);
		~ImportTableDialog();
		static QStringList splitLine(QTextStream * in, QString sep, QString q);

		bool update;
	private:
		//! Remember the originally requested name and schema
		QString m_tableName;
		QString m_schema;
		// and the originally active name (may be different)
		QTreeWidgetItem * m_activeItem;

		// We ought to be able use use parent() for this, but for some reason
		// qobject_cast<LiteManWindow*>(parent()) doesn't work
		LiteManWindow * creator;
		// true if altering currently active table
		bool m_alteringActive;

		void updateButton();
		char hexValue(QChar c);
		
	private slots:
		void fileButton_clicked();
		//! \brief Main import is handled here
		void slotAccepted();
		//! \brief Overloaded due the defined Qt signal/slot
		void createPreview(int i = 0);
		//! \brief Overloaded due the defined Qt signal/slot
		void createPreview(bool);
		//
		void setTablesForSchema(const QString & schema);
		void skipHeaderCheck_toggled(bool checked);
};

//! \brief A helper classes used for data import.
namespace ImportTable
{

	/*! \brief A base Model for all import "modules".
	It's a model in qt4 mvc architecture. See Qt4 docs for
	methods meanings.
	\author Petr Vanek <petr@scribus.info>
	*/
	class BaseModel : public QAbstractTableModel
	{
		Q_OBJECT

		public:
			BaseModel(QList<FieldInfo> fields, QObject * parent = 0);

			int rowCount(const QModelIndex & parent = QModelIndex()) const;
			int columnCount(const QModelIndex & parent = QModelIndex()) const;

			QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;

			QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

			//! \brief Number of columns in table;
			int m_columns;
			/*! \brief Internal structure of values.
			It's filled by format parsers in inherited classes */
			QList<QStringList> m_values;
			QStringList m_columnNames;
	};


	//! \brief Comma Separated Values importer
	class CSVModel : public BaseModel
	{
		Q_OBJECT

		public:
			CSVModel(QString fileName, QList<FieldInfo> fields, int skipHeader,
					 QString separator, QString quote,
					 QObject * parent = 0, int maxRows = 0);
	};

	/*! \brief MS Excel XML importer
	\note XML import requires Qt library at least in the 4.3.0 version.
	*/
	class XMLModel : public BaseModel
	{
		Q_OBJECT

		public:
			XMLModel(QString fileName, QList<FieldInfo> fields, int skipHeader,
					 QObject * parent = 0, int maxRows = 0);
	};

};

#endif
