/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QSqlDriver>
#include <QSqlQuery>
#include <QSqlError>
#include <QTextStream>
#include <QVariant>
#include <QFile>
#include <QMessageBox>

#include "database.h"
#include "preferences.h"
#include "shell.h"
#include "utils.h"
#include "sqlparser.h"

void Database::exception(const QString & message)
{
	QMessageBox::critical(0, tr("SQL Error"), message);
}

bool Database::execSql(QString statement)
{
	QSqlQuery query(statement, QSqlDatabase::database(SESSION_NAME));
	if(query.lastError().isValid())
	{
		exception(tr("Error executing: %1.").arg(query.lastError().text()));
		return false;
	}
	return true;
}

QString Database::sessionName(const QString & schema)
{
	return QString("%1_%2").arg(SESSION_NAME).arg(schema);
}

DbAttach Database::getDatabases()
{
	DbAttach ret;
	QSqlQuery query("PRAGMA database_list;", QSqlDatabase::database(SESSION_NAME));

	if (query.lastError().isValid())
	{
		exception(tr("Cannot get databases list. %1")
				  .arg(query.lastError().text()));
		return ret;
	}
	while(query.next())
		ret.insertMulti(query.value(1).toString(), query.value(2).toString());
	return ret;
}

SqlParser Database::parseTable(const QString & table, const QString & schema)
{
	// Build a query string to SELECT the CREATE statement from sqlite_master
	QString createSQL = QString("SELECT sql FROM ")
						+ getMaster(schema)
						+ " WHERE lower(name) = "
						+ Utils::quote(table).toLower()
						+ ";";
	// Run the query

	QSqlQuery createQuery(createSQL, QSqlDatabase::database(SESSION_NAME));
	// Make sure the query ran successfully
	if(createQuery.lastError().isValid()) {
		exception(tr("Error grabbing CREATE statement: ")
				  + table
				  + ": "
				  + createQuery.lastError().text());
	}
	// Position the Query on the first (only) result
	createQuery.first();
	// Grab the complete CREATE statement
	QString createStatement = createQuery.value(0).toString();

	// Parse the CREATE statement
	SqlParser parsed(createStatement);
#if 0 // this code only used for testing new parser
	qDebug("Parsing %s:", createStatement.toUtf8().data());
	qDebug("  m_isValid = %s", parsed.m_isValid ? "true" : "false");
	qDebug("  m_database = %s", parsed.m_database.toUtf8().data());
	qDebug("  m_name = %s", parsed.m_name.toUtf8().data());
	qDebug("  m_hasRowid = %s", parsed.m_hasRowid ? "true" : "false");
	qDebug("  m_isTable = %s", parsed.m_isTable ? "true" : "false");
	foreach (FieldInfo f, parsed.m_fields)
	{
		qDebug("  Field %s%s%s", f.name.toUtf8().data(),
			   f.type.isEmpty() ? "" : " type ",
			   f.type.toUtf8().data());
		qDebug("    %sDefault %s%s%s",
			   f.defaultValue.isEmpty() ? "No " : "",
			   f.defaultisQuoted ? "'" : "",
			   f.defaultValue.toUtf8().data(),
			   f.defaultisQuoted ? "'" : "");
		qDebug("    isPartOfPrimaryKey = %s",
			   f.isPartOfPrimaryKey ? "true" : "false");
		qDebug("    isAutoIncrement = %s",
			   f.isAutoIncrement ? "true" : "false");
		qDebug("    isNotNull = %s",
			   f.isNotNull ? "true" : "false");
	}
#endif

	return parsed;
}

QList<FieldInfo> Database::tableFields(const QString & table, const QString & schema)
{
	return parseTable(table, schema).m_fields;
}
QStringList Database::indexFields(const QString & index, const QString &schema)
{
	QString sql = QString("PRAGMA ")
				  + Utils::quote(schema)
				  + ".INDEX_INFO("
				  + Utils::quote(index)
				  + ");";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	QStringList fields;

	if (query.lastError().isValid())
	{
        exception(tr("Error while getting the fields of ")
        		  + index
        		  + ": "
        		  + query.lastError().text());
		return fields;
	}

	while (query.next())
		fields.append(query.value(2).toString());

	return fields;
}

DbObjects Database::getObjects(const QString type, const QString schema)
{
	DbObjects objs;

	QString sql;
	if (type.isNull())
	{
        sql = QString("SELECT name, tbl_name FROM ")
			  + getMaster(schema)
			  + ";";
	}
	else
	{
        sql = QString("SELECT name, tbl_name FROM ")
			  + getMaster(schema)
			  + " WHERE lower(type) = "
			  + Utils::literal(type).toLower()
			  + " and name not like 'sqlite_%';";
	}

	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	while(query.next())
		objs.insertMulti(query.value(1).toString(), query.value(0).toString());

	if(query.lastError().isValid())
		exception(tr("Error getting the list of ")
				  + type
				  + ": "
				  + query.lastError().text());

	return objs;
}

QStringList Database::getSysIndexes(const QString & table, const QString & schema)
{
	QStringList orig = Database::getObjects("index", schema).values(table);
	// really all indexes
	QStringList sysIx;
	QSqlQuery query(QString("PRAGMA ")
					+ Utils::quote(schema)
					+ ".index_list("
					+ Utils::quote(table)
					+ ");", QSqlDatabase::database(SESSION_NAME));

	QString curr;
	while(query.next())
	{
		curr = query.value(1).toString();
		if (!orig.contains(curr))
			sysIx.append(curr);
	}

	if(query.lastError().isValid())
		exception(tr("Error getting the list of indexes: ")
				  + query.lastError().text());

	return sysIx;
}

DbObjects Database::getSysObjects(const QString & schema)
{
	DbObjects objs;

    QSqlQuery query(QString("SELECT name, tbl_name FROM %1 "
							"WHERE type = 'table' and name like 'sqlite_%';")
					.arg(getMaster(schema)),
					QSqlDatabase::database(SESSION_NAME));

	if (schema.compare("temp", Qt::CaseInsensitive))
	{
		objs.insert("sqlite_master", "");
	}
	else
	{
		objs.insert("sqlite_temp_master", "");
	}
	while(query.next())
		objs.insertMulti(query.value(1).toString(), query.value(0).toString());

	if(query.lastError().isValid())
		exception(tr("Error getting the system catalogue: %1.").arg(query.lastError().text()));

	return objs;
}

bool Database::dropView(const QString & view, const QString & schema)
{
	QString sql = QString("DROP VIEW ")
				  + Utils::quote(schema)
				  + "."
				  + Utils::quote(view)
				  + ";";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if(query.lastError().isValid())
	{
		exception(tr("Error while dropping the view ")
				  + view
				  + ": "
				  + query.lastError().text());
		return false;
	}
	return true;
}

bool Database::dropIndex(const QString & name, const QString & schema)
{
	QString sql = QString("DROP INDEX ")
				  + Utils::quote(schema)
				  + "."
				  + Utils::quote(name)
				  + ";";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if(query.lastError().isValid())
	{
		exception(tr("Error while dropping the index ")
				  + name
				  + ": "
				  + query.lastError().text());
		return false;
	}
	return true;
}

bool Database::exportSql(const QString & fileName)
{
	QFile file(fileName);
	
	if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		exception(tr("Unable to open file %1 for writing.").arg(fileName));
		return false;
	}

	QTextStream stream(&file);
	
	// Run query for tables
	QString sql = "SELECT sql FROM sqlite_master;";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if (query.lastError().isValid())
	{
		exception(tr(
			"Error while exporting SQL: %1.").arg(query.lastError().text()));
		return false;
	}
	
	while(query.next())
		stream << query.value(0).toString() << ";\n";
	
	file.close();
	return true;
}

// TODO/FIXME: it definitely requires worker thread - to unfreeze GUI
bool Database::dumpDatabase(const QString & fileName)
{
	char * fname = fileName.toUtf8().data();
	FILE * f = fopen(fname, "w");
	
	if (f == NULL)
	{
		exception(tr("Unable to open file %1 for writing.").arg(fileName));
		return false;
	}

	sqlite3 * h = Database::sqlite3handle();
	Q_ASSERT_X(h!=0, "Database::dumpDatabase", "sqlite3handle is missing");
	ShellState sh;
	sh.db = h;
	sh.out = f;
	sh.nErr = 0;
	sh.writableSchema = 0;

	sqlite3_exec(sh.db, "PRAGMA writable_schema=ON", 0, 0, 0);
	fprintf(sh.out, "BEGIN TRANSACTION;\n");
	run_schema_dump_query(&sh,
        "SELECT name, type, sql FROM sqlite_master "
        "WHERE sql NOT NULL AND type=='table'");
	run_table_dump_query(&sh,
		"SELECT sql FROM sqlite_master "
        "WHERE sql NOT NULL AND type IN ('index','trigger','view')",
		(const char *)0);
	fprintf(sh.out, "COMMIT;\n");
	sqlite3_exec(sh.db, "PRAGMA writable_schema=OFF", 0, 0, 0);

	fclose(f);
	return true;
}

QString Database::describeObject(const QString & name,
								 const QString & schema,
								 const QString & type)
{
    QString sql = QString("select sql from ")
    			  + getMaster(schema)
    			  + " where lower(name) = "
    			  + Utils::quote(name).toLower()
    			  + " and type = \""
				  + type
				  + "\";";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if (query.lastError().isValid())
	{
		exception(tr("Error while describing object ")
				  + name
				  + ": "
				  + query.lastError().text());
		return "";
	}
	
	while(query.next())
		return query.value(0).toString();
	
	return "";
}

bool Database::dropTrigger(const QString & name, const QString & schema)
{
	QString sql = QString("DROP TRIGGER ")
				  + Utils::quote(schema)
				  + "."
				  + Utils::quote(name)
				  + ";";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if(query.lastError().isValid())
	{
		exception(tr("Error while dropping the trigger ")
				  + name
				  + ": "
				  + query.lastError().text());
		return false;
	}
	return true;
}

QString Database::hex(const QByteArray & val)
{
	const char hexdigits[] = {
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
	};
	QString ret("X'");
	for (int i = 0; i < val.size(); ++i)
	{
		ret += hexdigits[(val.at(i)>>4)&0xf];
		ret += hexdigits[val.at(i)&0xf];
	}
	return ret + "'";
}

QString Database::pragma(const QString & name)
{
	QString statement("PRAGMA main.%1;");
	QSqlQuery query(statement.arg(name), QSqlDatabase::database(SESSION_NAME));
	if (query.lastError().isValid())
	{
		exception(tr("Error executing: %1.").arg(query.lastError().text()));
		return "error";
	}

	while(query.next())
		return query.value(0).toString();
	return tr("Not Set");
}

sqlite3 * Database::sqlite3handle()
{
	QVariant v = QSqlDatabase::database(SESSION_NAME).driver()->handle();
	if (!v.isValid())
	{
		exception(tr("DB driver is not valid"));
		return 0;
	}
	if (qstrcmp(v.typeName(), "sqlite3*") != 0)
	{
		exception(tr("DB type name does not equal sqlite3"));
		return 0;
	}

	sqlite3 *handle = *static_cast<sqlite3 **>(v.data());
	if (handle == 0)
		exception(tr("DB handler is not valid"));

	return handle;
}

bool Database::setEnableExtensions(bool enable)
{
	sqlite3 * handle = Database::sqlite3handle();
	if (handle && sqlite3_enable_load_extension(handle, enable ? 1 : 0) != SQLITE_OK)
	{
		if (enable)
			exception(tr("Failed to enable extension loading"));
		else
			exception(tr("Failed to disable extension loading"));
		return false;
	}
	return true;
}

QStringList Database::loadExtension(const QStringList & list)
{
	sqlite3 * handle = Database::sqlite3handle();
	if (!handle)
		return QStringList();

	QStringList retval;

	foreach(QString f, list)
	{
		const char *zProc = 0;
		char *zErrMsg = 0;
		int rc;

		rc = sqlite3_load_extension(handle, f.toUtf8().data(), zProc, &zErrMsg);
		if (rc != SQLITE_OK)
		{
			exception(tr("Error loading extension\n")
					  + f
					  + "\n"
					  + QString::fromLocal8Bit(zErrMsg));
			sqlite3_free(zErrMsg);
		}
		else
			retval.append(f);
	}
	return retval;
}

QString Database::getMaster(const QString &schema)
{
    if (schema.compare(QString("temp"), Qt::CaseInsensitive)==0)
	{
		return QString("sqlite_temp_master");
	}
	else
	{
		return QString("%1.sqlite_master").arg(Utils::quote(schema));
	}
}

QString Database::getTempName(const QString & schema)
{
	QStringList existing = getObjects(QString(), schema).keys();
	int i = 0;
	while (existing.contains(QString("tmpname_%1").arg(i), Qt::CaseInsensitive))
	{
		++i;
	}
	return QString("tmpname_%1").arg(i);
}

bool Database::isAutoCommit()
{
	return sqlite3_get_autocommit(sqlite3handle()) != 0;
}

