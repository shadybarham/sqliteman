/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.

This file written by and copyright © 2015 Richard P. Parkins, M. A., and released under the GPL.

This class parses SQL schemas (not general SQL statements). Since the
schema is known to be valid, we do not always detect bad syntax.
*/

#ifndef SQLPARSER_H
#define SQLPARSER_H

enum tokenType {
	tokenIdentifier,
	tokenQuotedIdentifier,
	tokenStringLiteral,
	tokenBlobLiteral,
	tokenNumeric,
	tokenOperator,
	tokenSingle
};
typedef struct
{
	QString name;
	enum tokenType type;
} Token;


typedef struct
{
	QString name;
	QString type;
	QString defaultValue;
	bool defaultIsExpression;
	bool defaultisQuoted;
	bool isPartOfPrimaryKey;
	bool isAutoIncrement;
	bool isNotNull;
} FieldInfo;

/* When scanning for indexed-column items in a CREATE INDEX:-
 * name is a column name and fails if there is no such column
 * 'name' is a column name and fails if there is no such column
 * "name" is a column name if there is such a column, otherwise an expression
 * `name` is a column name and fails if there is no such column
 * [name] is a column name and fails if there is no such column
 * (name) is a column name and fails if there is no such column
 * 1 is a column name and fails if there is no such column
 * '1' is a column name and fails if there is no such column
 * "1" is a column name if there is such a column, otherwise an expression
 * `1` is a column name and fails if there is no such column
 * [1] is a column name and fails if there is no such column
 * (1) is an expression
 * When scanning a WHERE expression in a CREATE INDEX:-
 * name is a column name and fails if there is no such column
 * 'name' is a column name if there is such a column, otherwise an expression
 * "name" is a column name if there is such a column, otherwise an expression
 * `name` is a column name and fails if there is no such column
 * [name] is a column name and fails if there is no such column
 * (tested on sqlite 3.9.1)
 */
typedef struct
{
	QString name;
	QList<Token> expression;
} IndexedColumn;

typedef struct
{
	bool isUnique;
	QString name;
	QString tableName;
	
} IndexInfo;

class SqlParser
{
	public:
		bool m_isValid; // false -> couldn't parse CREATE statemnet
		QString m_database;
		QString m_name;
		bool m_hasRowid; // false -> WITHOUT ROWID (or a view)
		bool m_isTable; // false -> is a view
		QList<FieldInfo> m_fields;
	
		SqlParser(QString input);
		static QString defaultToken(FieldInfo &f);

	private:
		QList<Token> tokenise(QString input);
		void clearField(FieldInfo &f);
		void addToPrimaryKey(QString s);
};

#endif // SQLPARSER_H
