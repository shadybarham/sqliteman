/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QIcon>
#include <QComboBox>
#include <QPixmapCache>
#include <QItemSelection>
#include <QTreeWidgetItem>
#include <QTime>
#include <QUrl>

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
void Utils::dump(QTreeWidgetItem * item)
{
	item ? dump(*item) : qDebug("Null QTreeWidgetItem");
}
void Utils::dump(QComboBox & box)
{
	int n = box.count();
	if (n == 0)
	{
		qDebug("QComboBox with no items");
	}
	else
	{
		QString s;
		for (int i = 0; i < n; ++i)
		{
			if (i > 0) { s.append(", "); }
			s.append(box.itemText(i));
		}
		qDebug("%s", s.toUtf8().data());
	}
}
void Utils::dump(QComboBox * box)
{
	box ? dump(*box) : qDebug("Null QComboBox");
}
void Utils::dump(QVariant x)
{
	switch(x.type())
	{
		case QVariant::Invalid: qDebug("Invalid QVariant"); break;
		case QVariant::BitArray:
		case QVariant::Bitmap:
		case QVariant::Bool: qDebug(x.toBool() ? "true" : "false"); break;
		case QVariant::ByteArray:
		case QVariant::Char:
		case QVariant::Date:
		case QVariant::DateTime:
		case QVariant::Double:
		case QVariant::Int:
		case QVariant::LongLong:
		case QVariant::String:
		case QVariant::StringList:
		case QVariant::UInt:
		case QVariant::ULongLong:
			qDebug("%s", x.toString().toUtf8().data()); break;
		case QVariant::Brush: qDebug("Brush"); break;
		case QVariant::Color:
		{
			QColor c = x.value<QColor>();
			qDebug("Red %d, Blue %d, Green %d", c.red(), c.blue(), c.green());
			break;
		}
		case QVariant::Cursor: qDebug("Cursor"); break;
		case QVariant::EasingCurve: qDebug("EasingCurve"); break;
		case QVariant::Font:
		{
			QFont f = x.value<QFont>();
			qDebug("Font %s", f.key().toUtf8().data());
			break;
		}
		case QVariant::Hash: qDebug("Hash"); break;
		case QVariant::Icon: qDebug("Icon"); break;
		case QVariant::Image: qDebug("Image"); break;
		case QVariant::KeySequence:
		{
			QKeySequence k = x.value<QKeySequence>();
			qDebug("%s", k.toString().toUtf8().data());
			break;
		}
		case QVariant::Line:
		{
			QLine l = x.value<QLine>();
			qDebug("Line from (%d, %d) to (%d, %d)",
				   l.x1(), l.y1(), l.x2(), l.y2());
			break;
		}
		case QVariant::LineF:
		{
			QLineF l = x.value<QLineF>();
			qDebug("Line from (%f, %f) to (%f, %f)",
				   l.x1(), l.y1(), l.x2(), l.y2());
			break;
		}
		case QVariant::List:
			qDebug("List of length %d", x.toList().count()); break;
		case QVariant::Locale:
		{
			QLocale l = x.value<QLocale>();
			qDebug("%s", l.bcp47Name().toUtf8().data());
			break;
		}
		case QVariant::Map: qDebug("Map"); break;
		case QVariant::Matrix: qDebug("Matrix"); break;
		case QVariant::Transform: qDebug("Transform"); break;
		case QVariant::Matrix4x4: qDebug("Matrix4x4"); break;
		case QVariant::Palette: qDebug("Palette"); break;
		case QVariant::Pen: qDebug("Pen"); break;
		case QVariant::Pixmap: qDebug("Pixmap"); break;
		case QVariant::Point:
		{
			QPoint p = x.value<QPoint>();
			qDebug("(%d, %d)", p.x(), p.y());
			break;
		}
		case QVariant::PointF:
		{
			QPointF p = x.value<QPointF>();
			qDebug("(%f, %f)", p.x(), p.y());
			break;
		}
		case QVariant::Polygon:
		{
			QPolygon p = x.value<QPolygon>();
			qDebug("Array of %d points", p.count());
			break;
		}
		case QVariant::Quaternion: qDebug("Quaternion"); break;
		case QVariant::Rect:
		{
			QRect r = x.value<QRect>();
			qDebug("Rect from (%d, %d) to (%d, %d)",
				   r.left(), r.bottom(), r.right(), r.top());
			break;
		}
		case QVariant::RectF:
		{
			QRectF r = x.value<QRectF>();
			qDebug("Rect from (%f, %f) to (%f, %f)",
				   r.left(), r.bottom(), r.right(), r.top());
			break;
		}
		case QVariant::RegExp:
			qDebug("%s", x.toRegExp().pattern().toUtf8().data()); break;
		case QVariant::Region: qDebug("Region"); break;
		case QVariant::Size:
		{
			QSize s = x.value<QSize>();
			qDebug("Width %d, height %d", s.width(), s.height());
			break;
		}
		case QVariant::SizeF:
		{
			QSizeF s = x.value<QSizeF>();
			qDebug("Width %f, height %f", s.width(), s.height());
			break;
		}
		case QVariant::SizePolicy: qDebug("SizePolicy"); break;
		case QVariant::TextFormat: qDebug("TextFormat"); break;
		case QVariant::TextLength: qDebug("TextLength"); break;
		case QVariant::Time:
			qDebug("%s", x.toTime().toString().toUtf8().data()); break;
		case QVariant::Url:
			qDebug("%s", x.toUrl().toString().toUtf8().data()); break;
		case QVariant::Vector2D: qDebug("Vector2D"); break;
		case QVariant::Vector3D: qDebug("Vector3D"); break;
		case QVariant::Vector4D: qDebug("Vector4D"); break;
		case QVariant::UserType: qDebug("UserType"); break;
		default: qDebug("Unknown type"); break;
	}
}
