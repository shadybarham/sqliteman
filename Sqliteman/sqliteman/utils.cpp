/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QIcon>
#include <QPixmapCache>
#include <QItemSelection>
#include <QTreeWidgetItem>

#include "utils.h"


QIcon Utils::getIcon(const QString & fileName)
{
	QPixmap pm;

	if (! QPixmapCache::find(fileName, &pm))
	{
		QPixmap npm(QString(ICON_DIR) + "/" + fileName);
		QPixmapCache::insert(fileName, npm);
		return npm;
	}

	return pm;
}

QPixmap Utils::getPixmap(const QString & fileName)
{
	return QPixmap(QString(ICON_DIR) + "/" + fileName);
}

QString Utils::getTranslator(const QString & localeName)
{
	return QString(TRANSLATION_DIR)
		   + "/sqliteman_"
		   + localeName
		   + ".qm";
}

bool Utils::updateObjectTree(const QString & sql)
{
	if (sql.isNull())
		return false;
	QString tmp(sql.trimmed().toUpper());
	return (   tmp.startsWith("ALTER")
			|| tmp.startsWith("ATTACH")
			|| tmp.startsWith("CREATE")
			|| tmp.startsWith("DETACH")
			|| tmp.startsWith("DROP"));
}

bool Utils::updateTables(const QString & sql)
{
	if (sql.isNull())
		return false;
	QString tmp(sql.trimmed().toUpper());
	return (   tmp.startsWith("ALTER")
			|| tmp.startsWith("DELETE")
			|| tmp.startsWith("DETACH")
			|| tmp.startsWith("DROP")
			|| tmp.startsWith("INSERT")
			|| tmp.startsWith("REPLACE")
			|| tmp.startsWith("UPDATE"));
}

QString Utils::quote(QString s)
{
	return "\"" + s.replace("\"", "\"\"") + "\"";
}

QString Utils::quote(QStringList l)
{
	for (int i = 0; i < l.count(); ++i)
	{
		l.replace(i, l[i].replace("\"", "\"\""));
	}
	return "\"" + l.join("\", \"") + "\"";
}

QString Utils::literal(QString s)
{
	return "'" + s.replace("'", "''") + "'";
}

QString Utils::unQuote(QString s)
{
	if (s.startsWith("'")) {
		s.replace("''", "'");
		return s.mid(1, s.size() - 2);
	}
	else if (s.startsWith("\"")) {
		s.replace("\"\"", "\"");
		return s.mid(1, s.size() - 2);
	}
	else { return s; }
}

QString Utils::like(QString s)
{
	return "'%"
		   + s.replace("'", "''")
			  .replace("_", "@_")
			  .replace("%", "@%")
			  .replace("@", "@@")
		   + "%' ESCAPE '@'";
}

// debugging hacks
void Utils::dump(QItemSelection selection)
{
	QModelIndexList list = selection.indexes();
	int n = list.count();
	if (n == 0)
	{
		qDebug("selection is empty");
	}
	else for (int i = 0; i < n; ++i)
	{
		QModelIndex index = list.at(i);
		qDebug("row %d column %d", index.row(), index.column());
	}
}
void Utils::dump(QTreeWidgetItem & item)
{
	qDebug("text(0) \"%s\", text(1) \"%s\"",
		   item.text(0).toUtf8().data(), item.text(1).toUtf8().data());
}
