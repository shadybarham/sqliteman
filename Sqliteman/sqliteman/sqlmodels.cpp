/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.

If table name contains non-alphanumeric characters, no rows are displayed,
although they are actually still there as proved by renaming it back again.
This is a QT bug.

*/
#include <time.h>

#include <QColor>
#include <QSqlError>
#include <QSqlField>
#include <QSqlQuery>

#include "sqlmodels.h"
#include "database.h"
#include "preferences.h"
#include "utils.h"


SqlTableModel::SqlTableModel(QObject * parent, QSqlDatabase db)
	: QSqlTableModel(parent, db),
	m_pending(false),
	m_schema(""),
	m_useCount(1)
{
	m_deleteCache.clear();
	m_insertCache.clear();
	Preferences * prefs = Preferences::instance();
	switch (prefs->rowsToRead())
	{
		case 0: m_readRowsCount = 256; break;
		case 1: m_readRowsCount = 512; break;
		case 2: m_readRowsCount = 1024; break;
		case 3: m_readRowsCount = 2048; break;
		case 4: m_readRowsCount = 4096; break;
		default: m_readRowsCount = 0; break;
	}
	connect(this, SIGNAL(primeInsert(int, QSqlRecord &)),
			this, SLOT(doPrimeInsert(int, QSqlRecord &)));
}

QVariant SqlTableModel::data(const QModelIndex & item, int role) const
{
	QVariant rawdata = QSqlTableModel::data(item, Qt::DisplayRole);
	QString curr(rawdata.toString());
	// numbers
	if (role == Qt::TextAlignmentRole)
	{
		bool ok;
		curr.toDouble(&ok);
		if (ok)
			return QVariant(Qt::AlignRight | Qt::AlignTop);
		return QVariant(Qt::AlignTop);
	}

	if (role == Qt::BackgroundColorRole)
    {
		// QSqlTableModel::isDirty() always returns true for an inserted
		// row but we only want to show it as modified if user has changed
		// it since insertion.
		int row = item.row();
		if (m_insertCache.contains(row))
		{
			if (m_insertCache.value(row)) { return QVariant(Qt::cyan); }
		}
		else
		{
	        for (int i = 0; i < columnCount(); ++i)
	        {
	            if (isDirty(index(row, i))) { return QVariant(Qt::cyan); }
	        }
		}
    }

	Preferences * prefs = Preferences::instance();
	bool useNull = prefs->nullHighlight();
	QColor nullColor = prefs->nullHighlightColor();
	QString nullText = prefs->nullHighlightText();
	bool useBlob = prefs->blobHighlight();
	QColor blobColor = prefs->blobHighlightColor();
	QString blobText = prefs->blobHighlightText();
	bool cropColumns = prefs->cropColumns();

	// nulls
	if (curr.isNull())
	{
		if (role == Qt::ToolTipRole)
			return QVariant(tr("NULL value"));
		if (useNull)
		{
			if (role == Qt::BackgroundColorRole)
				return QVariant(nullColor);
			if (role == Qt::DisplayRole)
				return QVariant(nullText);
		}
	}

	// blobs
	if (rawdata.type() == QVariant::ByteArray)
	{
		if (role == Qt::ToolTipRole) { return QVariant(tr("BLOB value")); }
		if (useBlob)
		{
			if (role == Qt::BackgroundColorRole) { return QVariant(blobColor); }
			if (role == Qt::DisplayRole) { return QVariant(blobText); }
		}
		else if (role == Qt::DisplayRole)
		{
			curr = Database::hex(rawdata.toByteArray());
			if (!cropColumns) { return QVariant(curr); }
		}
	}

	if (role == Qt::BackgroundColorRole)
	{
		return QColor(255, 255, 255);
	}

	// advanced tooltips
	if (role == Qt::ToolTipRole)
		return QVariant("<qt>" + curr + "</qt>");

	if (role == Qt::DisplayRole && cropColumns)
		return QVariant(curr.length() > 20 ? curr.left(20)+"..." : curr);

	return QSqlTableModel::data(item, role);
}

bool SqlTableModel::setData ( const QModelIndex & ix, const QVariant & value, int role)
{
	if (QSqlTableModel::setData(ix, value, role))
	{
		if (role == Qt::EditRole)
		{
			m_pending = true;
			int row = ix.row();
			if (m_insertCache.contains(row))
			{
				m_insertCache.insert(row, true);
			}
		}
		return true;
	}
	else { return false; }
}

QVariant SqlTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal)
	{
		switch (role)
		{
			case (Qt::DecorationRole):
				switch (m_header[section])
				{
					case (SqlTableModel::PK):
						return Utils::getIcon("key.png");
						break;
					case (SqlTableModel::Auto):
						return Utils::getIcon("index.png");
						break;
					case (SqlTableModel::Default):
						return Utils::getIcon("column.png");
						break;
					default:
						return 0;
				}
				break;
			case (Qt::ToolTipRole):
				switch (m_header[section])
				{
					case (SqlTableModel::PK):
						return tr("Primary Key");
						break;
					case (SqlTableModel::Auto):
						return tr("Autoincrement");
						break;
					case (SqlTableModel::Default):
						return tr("Has default value");
						break;
					default:
						return "";
				}
				break;
		}
	}
	return QSqlTableModel::headerData(section, orientation, role);
}

bool SqlTableModel::insertRowIntoTable(const QSqlRecord &values)
{
	bool generated = false;
	for (int i = 0; i < values.count(); ++i)
	{
		generated |= values.isGenerated(i);
	}
	if (generated)
	{
		return QSqlTableModel::insertRowIntoTable(values);
	}
	// QSqlTableModel::insertRowIntoTable will fail, so we do the right thing...
	QString sql("INSERT INTO ");
	sql += Utils::q(m_schema) + "." + Utils::q(tableName());
	sql += " DEFAULT VALUES;";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	return !query.lastError().isValid();
}

void SqlTableModel::doPrimeInsert(int row, QSqlRecord & record)
{
	QList<FieldInfo> fl = Database::tableFields(tableName(), m_schema);
	bool ok;
	QString defval;
	// guess what type is the default value.
	foreach (FieldInfo column, fl)
	{
		if (column.defaultValue.isEmpty())
		{
			// prevent integer type being displayed as 0
			record.setValue(column.name, QVariant());
		}
		else if (column.defaultisQuoted)
		{
			record.setValue(column.name, QVariant(column.defaultValue));
		}
		else if (column.defaultIsExpression)
		{
			record.setValue(column.name, QVariant(column.defaultValue));
		}
		else
		{
			char s[22];
			time_t dummy;
			defval = column.defaultValue;
			if (defval.compare("CURRENT_TIMESTAMP", Qt::CaseInsensitive) == 0)
			{
				time(&dummy);
				(void)strftime(s, 20, "%F %T", localtime(&dummy));
				record.setValue(column.name, QVariant(s));
			}
			else if (defval.compare("CURRENT_TIME", Qt::CaseInsensitive) == 0)
			{
				time(&dummy);
				(void)strftime(s, 20, "%T", localtime(&dummy));
				record.setValue(column.name, QVariant(s));
			}
			else if (defval.compare("CURRENT_DATE", Qt::CaseInsensitive) == 0)
			{
				time(&dummy);
				(void)strftime(s, 20, "%F", localtime(&dummy));
				record.setValue(column.name, QVariant(s));
			}
			else
			{
				int i = defval.toInt(&ok);
				if (ok)
				{
					record.setValue(column.name, QVariant(i));
				}
				else
				{
					double d = defval.toDouble(&ok);
					if (ok)
					{
						record.setValue(column.name, QVariant(d));
					}
					else
					{
						record.setValue(column.name, QVariant(defval));
					}
				}
			}
		}
	}
}

bool SqlTableModel::insertRows ( int row, int count, const QModelIndex & parent)
{
	if (QSqlTableModel::insertRows(row, count, parent))
	{
		m_pending = true;
		for (int i = 0; i < count; ++i)
		{
			m_insertCache.insert(row + i, false);
		}
		return true;
	}
	else { return false; }
}

bool SqlTableModel::removeRows ( int row, int count, const QModelIndex & parent)
{
	// this is a workaround to allow mark heading as deletion
	// (as it's propably a bug in Qt QSqlTableModel ManualSubmit handling
	if (QSqlTableModel::removeRows(row, count, parent))
	{
		m_pending = true;
		emit dataChanged(index(row, 0), index(row+count-1, columnCount()-1));
		emit headerDataChanged(Qt::Vertical, row, row+count-1);
		for (int i = 0; i < count; ++i)
		{
			m_deleteCache.append(row+i);
			m_insertCache.remove(row+i);
		}
		return true;
	}
	else { return false; }
}

bool SqlTableModel::isDeleted(int row)
{
	return m_deleteCache.contains(row);
}

bool SqlTableModel::isNewRow(int row)
{
	return m_insertCache.contains(row) && !m_insertCache.value(row);
}

void SqlTableModel::setTable(const QString &tableName)
{
	m_header.clear();
	m_deleteCache.clear();
	m_insertCache.clear();
	QStringList indexes = Database::getSysIndexes(tableName, m_schema);
	QList<FieldInfo> columns = Database::tableFields(tableName, m_schema);

	int colnum = 0;
	foreach (FieldInfo c, columns)
	{
		if (c.isPartOfPrimaryKey)
		{
			m_header[colnum] =
				(c.isAutoIncrement) ? SqlTableModel::Auto : SqlTableModel::PK;
			++colnum;
			continue;
		}
		// show has default icon
		if (!c.defaultValue.isEmpty())
		{
			m_header[colnum] = SqlTableModel::Default;
			++colnum;
			continue;
		}
		m_header[colnum] = SqlTableModel::None;
		++colnum;
	}
	if (m_schema.isEmpty())
	{
		QSqlTableModel::setTable(tableName);
	}
	else
	{
		QSqlTableModel::setTable(m_schema + "." + tableName);
	}
}

void SqlTableModel::detach (SqlTableModel * model)
{
	if (--(model->m_useCount) == 0) { delete model ; }
}

void SqlTableModel::fetchAll()
{
	if (rowCount() > 0)
	{
		while (canFetchMore(QModelIndex()))
		{
			fetchMore();
		}
	}
}

bool SqlTableModel::select()
{
	bool result = QSqlTableModel::select();
	while (   result &&
			  canFetchMore(QModelIndex())
		   && (   (m_readRowsCount == 0)
			   || (rowCount() < m_readRowsCount)))
	{
		fetchMore();
	}
	return result;
}

void SqlTableModel::setPendingTransaction(bool pending)
{
	m_pending = pending;

	if (!pending)
	{
		for (int i = 0; i < m_deleteCache.size(); ++i)
		{
			emit headerDataChanged(
				Qt::Vertical, m_deleteCache[i], m_deleteCache[i]);
		}
		m_deleteCache.clear();
		m_insertCache.clear();
	}
}

bool SqlTableModel::deleteRowFromTable(int row)
{
	bool result = QSqlTableModel::deleteRowFromTable(row);
	if (result) { emit reallyDeleting(row); }
	return result;
}


SqlQueryModel::SqlQueryModel( QObject * parent)
	: QSqlQueryModel(parent),
	m_useCount(1)
{
	Preferences * prefs = Preferences::instance();
	switch (prefs->rowsToRead())
	{
		case 0: m_readRowsCount = 256; break;
		case 1: m_readRowsCount = 512; break;
		case 2: m_readRowsCount = 1024; break;
		case 3: m_readRowsCount = 2048; break;
		case 4: m_readRowsCount = 4096; break;
		default: m_readRowsCount = 0; break;
	}
}

QVariant SqlQueryModel::data(const QModelIndex & item, int role) const
{
	QVariant rawdata = QSqlQueryModel::data(item, Qt::DisplayRole);
	QString curr(rawdata.toString());

	// numbers
	if (role == Qt::TextAlignmentRole)
	{
		bool ok;
		curr.toDouble(&ok);
		if (ok)
			return QVariant(Qt::AlignRight | Qt::AlignTop);
		return QVariant(Qt::AlignTop);
	}

	Preferences * prefs = Preferences::instance();
	bool useNull = prefs->nullHighlight();
	QColor nullColor = prefs->nullHighlightColor();
	QString nullText = prefs->nullHighlightText();
	bool useBlob = prefs->blobHighlight();
	QColor blobColor = prefs->blobHighlightColor();
	QString blobText = prefs->blobHighlightText();
	bool cropColumns = prefs->cropColumns();

	if (useNull && curr.isNull())
	{
		if (role == Qt::BackgroundColorRole)
			return QVariant(nullColor);
		if (role == Qt::ToolTipRole)
			return QVariant(tr("NULL value"));
		if (role == Qt::DisplayRole)
			return QVariant(nullText);
	}

	if (useBlob && (rawdata.type() == QVariant::ByteArray))
	{
		if (role == Qt::BackgroundColorRole)
			return QVariant(blobColor);
		if (role == Qt::ToolTipRole)
			return QVariant(tr("BLOB value"));
		if (   (role == Qt::DisplayRole)
			|| (role == Qt::EditRole))
		{
			return QVariant(blobText);
		}
	}

	if (role == Qt::BackgroundColorRole)
	{
		return QColor(255, 255, 255);
	}

	// advanced tooltips
	if (role == Qt::ToolTipRole)
		return QVariant("<qt>" + curr + "</qt>");

	if (role == Qt::DisplayRole && cropColumns)
		return QVariant(curr.length() > 20 ? curr.left(20)+"..." : curr);

	return QSqlQueryModel::data(item, role);
}

void SqlQueryModel::setQuery ( const QSqlQuery & query )
{
	QSqlQueryModel::setQuery(query);
	info = record();
	if (columnCount() > 0)
	{
		while (   canFetchMore(QModelIndex())
			   && (   (m_readRowsCount == 0)
				   || (rowCount() < m_readRowsCount)))
		{
			fetchMore();
		}
	}
}

void SqlQueryModel::setQuery ( const QString & query, const QSqlDatabase & db)
{
	QSqlQueryModel::setQuery(query, db);
	info = record();
	if (columnCount() > 0)
	{
		while (   canFetchMore(QModelIndex())
			   && (   (m_readRowsCount == 0)
				   || (rowCount() < m_readRowsCount)))
		{
			fetchMore();
		}
	}
}

void SqlQueryModel::detach (SqlQueryModel * model)
{
	if (--(model->m_useCount) == 0) { delete model ; }
}

void SqlQueryModel::fetchAll()
{
	if (rowCount() > 0)
	{
		while (canFetchMore(QModelIndex()))
		{
			fetchMore();
		}
	}
}
