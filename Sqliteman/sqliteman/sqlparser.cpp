/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.

This file written by and copyright © 2015 Richard P. Parkins, M. A., and released under the GPL.

It contains a parser for SQL schemas (not general SQL statements). Since the
schema is known to be valid, we do not detect all cases of bad syntax.

sqlite_master and describe table leave any quotes around types, but pragma table_info removes them.
*/

#include "utils.h"
#include "sqlparser.h"

// state machine tokeniser
QList<Token> SqlParser::tokenise(QString input)
{
	static const QString hexDigit("0123456789ABCDEFabcdef");
	QList<Token> result = QList<Token>();
	while (!input.isEmpty())
	{
		Token t;
		int state = 0; // nothing
		while (!input.isEmpty())
		{
			QChar c = input.at(0);
			switch (state)
			{
				case 0: // nothing
					if (c.isSpace())
					{
						input.remove(0, 1);
						continue;
					} // ignore it
					else if (c == '0')
					{
						state = 1; // had initial 0
					}
					else if (c.isDigit())
					{
						state = 3; // in number, no . or E yet
					}
					else if (c == '.')
					{
						state = 2; // had initial .
					}
					else if (c.toUpper() == 'X')
					{
						state = 8; // blob literal or identifier
					}
					else if (c.isLetter() || (c == '_'))
					{
						state = 10; // identifier
					}
					else if (c == '"')
					{
						t.name = QString(""); // empty, not null
						state = 11; // "quoted identifier"
						input.remove(0, 1);
						continue;
					}
					else if (c == '\'')
					{
						t.name = QString(""); // empty, not null
						state = 13; // 'string literal'
						input.remove(0, 1);
						continue;
					}
					else if (c == '|')
					{
						state = 15; // check for ||
					}
					else if (c == '<')
					{
						state = 16; // check for << <> <=
					}
					else if (c == '>')
					{
						state = 17; // check for >> >=
					}
					else if (c == '=')
					{
						state = 18; // check for ==
					}
					else if (c == '!')
					{
						state = 19; // !=
					}
					else if (c == '[')
					{
						t.name = QString(""); // empty, not null
						state = 20; // [quoted identifier]
						input.remove(0, 1);
						continue;
					}
					else if (c == '`')
					{
						t.name = QString(""); // empty, not null
						state = 21; // `quoted identifier`
						input.remove(0, 1);
						continue;
					}
					else
					{
						// single character token
						t.name.append(c);
						t.type = tokenSingle;
						input.remove(0, 1);
						break;
					}
					t.name.append(c);
					input.remove(0, 1);
					continue;
				case 1: // had initial 0
					if (c.isDigit())
					{
						state = 3; // in number, no . or E yet
					}
					else if (c.toUpper() == 'X')
					{
						state = 7; // in hex literal
					}
					else if (c == '.')
					{
						state = 4; // in number, had .
					}
					else if (c.toUpper() == 'E')
					{
						state = 5; // in number, just had E
					}
					else { // token is just 0
						t.type = tokenNumeric;
						break;
					}
					t.name.append(c);
					input.remove(0, 1);
					continue;
				case 2: // had initial .
					if (c.isDigit())
					{
						state = 4; // in number, had .
					}
					else { // token is just .
						t.type = tokenSingle;
						break;
					}
					t.name.append(c);
					input.remove(0, 1);
					continue;
				case 3: // in number, no . or E yet
					if (c == '.')
					{
						state = 4; // in number, had .
					}
					else if (c.toUpper() == 'E')
					{
						state = 5; // in number, just had E
					}
					else if (!c.isDigit())
					{
						t.type = tokenNumeric;
						break; // end of number
					}
					t.name.append(c);
					input.remove(0, 1);
					continue;
				case 4: // in number, had .
					if (c.toUpper() == 'E')
					{
						state = 5; // in number, just had E
					}
					else if (!c.isDigit())
					{
						t.type = tokenNumeric;
						break; // end of number
					}
					t.name.append(c);
					input.remove(0, 1);
					continue;
				case 5: // in number, just had E
					if (c.isDigit() || (c == '-') || (c == '+'))
					{
						state = 6; // in number after E
						t.name.append(c);
						input.remove(0, 1);
						continue;
					}
					break; // actually invalid, but end number
				case 6: // in number after E
					if (c.isDigit())
					{
						t.name.append(c);
						input.remove(0, 1);
						continue;
					}
					t.type = tokenNumeric;
					break; // end number
				case 7: // in hex literal
					if (hexDigit.contains(c))
					{
						t.name.append(c);
						input.remove(0, 1);
						continue;
					}
					t.type = tokenNumeric;
					break; // end (hex) number
				case 8: // blob literal or identifier
					if (c == '\'')
					{
						state = 9; // blob literal
					}
					else if (c.isLetterOrNumber() || (c == '_') || (c == '$'))
					{
						state = 10; // identifier
					}
					else { // identifier just X
						t.type = tokenIdentifier;
						break;
					}
					t.name.append(c);
					input.remove(0, 1);
					continue;
				case 9: // blob literal
					if (c == '\'')
					{
						t.name.append(c);
						input.remove(0, 1);
						t.type = tokenBlobLiteral;
						break; // end of blob literal
					}
					else if (hexDigit.contains(c))
					{
						t.name.append(c);
						input.remove(0, 1);
						continue;
					}
					else { // invalid end of blob literal
						t.type = tokenBlobLiteral;
						break;
					}
				case 10: // identifier
					if (c.isLetterOrNumber() || (c == '_') || (c == '$'))
					{
						t.name.append(c);
						input.remove(0, 1);
						continue;
					}
					else { // end of identifier
						t.type = tokenIdentifier;
						break;
					}
				case 11: // "quoted identifier"
					if (c == '"')
					{
						state = 12; // look for doubled "
					}
					else
					{
						t.name.append(c);
					}
					input.remove(0, 1);
					continue;
				case 12: // look for doubled "
					if (c == '"')
					{
						state = 11; // quoted identifier
						t.name.append(c);
						input.remove(0, 1);
						continue;
					}
					else { // end of quoted identifier
						t.type = tokenQuotedIdentifier;
						break;
					}
				case 13: // 'string literal'
					if (c == '\'')
					{
						state = 14; // look for doubled '
					}
					else
					{
						t.name.append(c);
					}
					input.remove(0, 1);
					continue;
				case 14: // look for doubled '
					if (c == '\'')
					{
						state = 13; // string literal
						t.name.append(c);
						input.remove(0, 1);
						continue;
					}
					else { // end of string literal
						t.type = tokenStringLiteral;
						break;
					}
				case 15: // check for ||
					if (c == '|')
					{
						t.name.append(c);
						input.remove(0, 1);
					}
					t.type = tokenOperator;
					break;
				case 16: // check for << <> <=
					if ((c == '<') || (c == '>') || (c == '='))
					{
						t.name.append(c);
						input.remove(0, 1);
					}
					t.type = tokenOperator;
					break;
				case 17: // check for >> >=
					if ((c == '>') || (c == '='))
					{
						t.name.append(c);
						input.remove(0, 1);
					}
					t.type = tokenOperator;
					break;
				case 18: // check for ==
					if (c == '=')
					{
						t.name.append(c);
						input.remove(0, 1);
					}
					t.type = tokenOperator;
					break;
				case 19: // !=
					// ! without = is invalid, but we treat it as a token
					t.name.append(c);
					input.remove(0, 1);
					t.type = tokenSingle;
					break;
				case 20: // [quoted identifier]
					if (c == ']')
					{
						t.type = tokenQuotedIdentifier;
						input.remove(0, 1);
						break;
					}
					t.name.append(c);
					input.remove(0, 1);
					continue;
				case 21: // `quoted identifier`
					if (c == '`')
					{
						t.type = tokenQuotedIdentifier;
						input.remove(0, 1);
						break;
					}
					t.name.append(c);
					input.remove(0, 1);
					continue;
			}
			// if we didn't continue, we're at the end of the token
			break;
		}
		result.append(t);
	}
	return result;
}

void SqlParser::clearField(FieldInfo &f)
{
	f.name = QString();
	f.type = QString();
	f.defaultValue = QString();
	f.defaultIsExpression = false;
	f.defaultisQuoted = false;
	f.isPartOfPrimaryKey = false;
	f.isAutoIncrement = false;
	f.isNotNull = false;
}

void SqlParser::addToPrimaryKey(QString s)
{
	for (int i = 0; i < m_fields.count(); ++i)
	{
		if (s.compare(m_fields.at(i).name, Qt::CaseInsensitive) == 0)
		{
			m_fields[i].isPartOfPrimaryKey = true;
		}
	}
}

SqlParser::SqlParser(QString input)
{
	int depth = 0;
	QList<Token> tokens = tokenise(input);
	m_hasRowid = true;
	int state = 0; // nothing
	m_isValid = false;
	FieldInfo f;
	clearField(f);
	while (!tokens.isEmpty())
	{
		QString s(tokens.at(0).name);
		switch (state)
		{
			case 0: // nothing
				if (s.compare("CREATE", Qt::CaseInsensitive) == 0)
				{
					state = 1; // seen CREATE
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 1: // seen CREATE
				// TEMP[ORARY] doesn't get copied to schema
				if (s.compare("TABLE", Qt::CaseInsensitive) == 0)
				{
					state = 2; // CREATE TABLE
					m_isTable = true;
				}
#if 0 // for now, we don't parse CREATE VIEW statements
				else if (s.compare("VIEW", Qt::CaseInsensitive) == 0)
				{
					state = ?; // CREATE VIEW
					m_isTemp = false;
					m_isTable = false;
				}
#endif
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 2: // CREATE TABLE, CREATE VIEW
				// IF NOT EXISTS doesn't get copied to schema
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					m_name = s;
					state = 3; // had database or table name
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 3:
				if (s.compare(".") == 0)
				{
					m_database = m_name;
					state = 4; // look for table name after <database>.
				}
				else if ((s.compare("(") == 0) && m_isTable)
				{
					state = 6; // CREATE TABLE ( <columns...>
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 4: // look for table name after dot
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					m_name = s;
					state = 5; // look for (
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 5: // look for (
				if (   (s.compare("(") == 0) && m_isTable)
				{
					state = 6; // CREATE TABLE ( <columns...>
				}
				else { break; } // not a valid create table statement
				tokens.removeFirst();
				continue;
			case 6: // look for a column definition
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					f.name = s;
					state = 8; // look for type name or constraint or , or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 7: // look for column definition or table constraint
				if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0)
				{
					state = 41; // look for table constraint name
				}
				else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0)
				{
					state = 43; // look for KEY
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 50; // look for columns and conflict clause
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 59; // look for bracketed expression
				}
				else if (s.compare("FOREIGN", Qt::CaseInsensitive) == 0)
				{
					state = 61; // look for KEY
				}
				else if (   (tokens.at(0).type == tokenIdentifier)
						 || (tokens.at(0).type == tokenQuotedIdentifier)
						 || (tokens.at(0).type == tokenStringLiteral))
				{
					f.name = s;
					state = 8; // look for type name or constraint or , or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 8: // look for type name or column constraint or , or )
				if (s.compare(",") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 7;
				}
				else if (s.compare(")") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0)
				{
					state = 10; // look for column constraint name
				}
				else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0)
				{
					state = 12; // look for KEY
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 18; // look for NULL
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 19; // look for conflict clause or next
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 22; // look for bracketed expression
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 24; // look for default value
				}
				else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 27; // look for collation name
				}
				else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 28; // look for (foreign) table name
				}
				else if (   (tokens.at(0).type == tokenIdentifier)
						 || (tokens.at(0).type == tokenQuotedIdentifier)
						 || (tokens.at(0).type == tokenStringLiteral))
				{
					f.type = s;
					state = 9; // look for constraint or , or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 9: // look for column constraint or , or )
				if (s.compare(",") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 7; // look for column definition or table constraint
				}
				else if (s.compare(")") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0)
				{
					state = 10; // look for column constraint name
				}
				else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0)
				{
					state = 12; // look for KEY
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 18; // look for NULL
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 19; // look for conflict clause or next
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 22; // look for bracketed expression
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 24; // look for default value
				}
				else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 27; // look for collation name
				}
				else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 28; // look for (foreign) table name
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 10: // look for column constraint name
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					state = 11; // look for constraint or , or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 11: // look for constraint
				if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0)
				{
					state = 12; // look for KEY
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 18; // look for NULL
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 19; // look for conflict clause or next
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 22; // look for bracketed expression
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 24; // look for default value
				}
				else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 27; // look for collation name
				}
				else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 28; // look for (foreign) table name
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 12: // look for KEY
				if (s.compare("KEY", Qt::CaseInsensitive) == 0)
				{
					f.isPartOfPrimaryKey = true;
					state = 13; // look for ASC or DESC or conflict clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 13: // look for ASC or DESC or conflict clause
				if (s.compare("ASC", Qt::CaseInsensitive) == 0)
				{
					state = 14; // look for conflict clause
				}
				else if (s.compare("DESC", Qt::CaseInsensitive) == 0)
				{
					state = 14; // look for conflict clause
				}
				else if (s.compare("ON", Qt::CaseInsensitive) == 0)
				{
					state = 15; // look for CONFLICT
				}
				else if (s.compare("AUTOINCREMENT", Qt::CaseInsensitive) == 0)
				{
					f.isAutoIncrement = true;
					state = 9; // look for column constraint or , or )
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 18; // look for NULL
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 19; // look for conflict clause or next
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 22; // look for bracketed expression
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 24; // look for default value
				}
				else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 27; // look for collation name
				}
				else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 28; // look for (foreign) table name
				}
				else if (s.compare(",") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 7;
				}
				else if (s.compare(")") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 14: // look for conflict clause
				if (s.compare("ON", Qt::CaseInsensitive) == 0)
				{
					state = 15; // look for CONFLICT
				}
				else if (s.compare("AUTOINCREMENT", Qt::CaseInsensitive) == 0)
				{
					f.isAutoIncrement = true;
					state = 9; // look for column constraint or , or )
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 18; // look for NULL
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 19; // look for conflict clause or next
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 22; // look for bracketed expression
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 24; // look for default value
				}
				else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 27; // look for collation name
				}
				else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 28; // look for (foreign) table name
				}
				else if (s.compare(",") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 7;
				}
				else if (s.compare(")") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 15: // look for CONFLICT
				if (s.compare("CONFLICT", Qt::CaseInsensitive) == 0)
				{
					state = 16; // look for conflict action
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 16: // look for conflict action
				if (s.compare("ROLLBACK", Qt::CaseInsensitive) == 0)
				{
					state = 17; // look for AUTOINCREMENT or next
				}
				else if (s.compare("ABORT", Qt::CaseInsensitive) == 0)
				{
					state = 17; // look for AUTOINCREMENT or next
				}
				else if (s.compare("FAIL", Qt::CaseInsensitive) == 0)
				{
					state = 17; // look for AUTOINCREMENT or next
				}
				else if (s.compare("IGNORE", Qt::CaseInsensitive) == 0)
				{
					state = 17; // look for AUTOINCREMENT or next
				}
				else if (s.compare("REPLACE", Qt::CaseInsensitive) == 0)
				{
					state = 17; // look for AUTOINCREMENT or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 17: // look for AUTOINCREMENT or next
				if (s.compare("AUTOINCREMENT", Qt::CaseInsensitive) == 0)
				{
					f.isAutoIncrement = true;
					state = 9; // look for column constraint or , or )
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 18; // look for NULL
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 19; // look for conflict clause or next
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 22; // look for bracketed expression
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 24; // look for default value
				}
				else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 27; // look for collation name
				}
				else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 28; // look for (foreign) table name
				}
				else if (s.compare(",") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 7;
				}
				else if (s.compare(")") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 18: // look for NULL
				if (s.compare("NULL", Qt::CaseInsensitive) == 0)
				{
					f.isNotNull = true;
					state = 19; // look for conflict clause or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 19: // look for conflict clause or next
				if (s.compare("ON", Qt::CaseInsensitive) == 0)
				{
					state = 20; // look for CONFLICT
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 18; // look for NULL
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 19; // look for conflict clause or next
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 22; // look for bracketed expression
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 24; // look for default value
				}
				else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 27; // look for collation name
				}
				else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 28; // look for (foreign) table name
				}
				else if (s.compare(",") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 7;
				}
				else if (s.compare(")") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 20: // look for CONFLICT
				if (s.compare("CONFLICT", Qt::CaseInsensitive) == 0)
				{
					state = 21; // look for conflict action
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 21: // look for conflict action
				if (s.compare("ROLLBACK", Qt::CaseInsensitive) == 0)
				{
					state = 9; // look for constraint or , or )
				}
				else if (s.compare("ABORT", Qt::CaseInsensitive) == 0)
				{
					state = 9; // look for constraint or , or )
				}
				else if (s.compare("FAIL", Qt::CaseInsensitive) == 0)
				{
					state = 9; // look for constraint or , or )
				}
				else if (s.compare("IGNORE", Qt::CaseInsensitive) == 0)
				{
					state = 9; // look for constraint or , or )
				}
				else if (s.compare("REPLACE", Qt::CaseInsensitive) == 0)
				{
					state = 9; // look for constraint or , or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 22: // look for bracketed expression
				if (s.compare("(") == 0)
				{
					state = 23; // look for end of expression
					++depth;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 23: // look for end of expression
				if (s.compare("(") == 0)
				{
					++depth;
				}
				else if (s.compare(")") == 0)
				{
					if (--depth == 0) { state = 9; }
				}
				tokens.removeFirst();
				continue;
			case 24: // look for default value
				if (s.compare("(") == 0)
				{
					f.defaultIsExpression = true;
					f.defaultValue = "(";
					state = 25; // scan for end of expression
					++depth;
				}
				else if (   (s.compare("+") == 0)
						 || (s.compare("-") == 0))
				{
					f.defaultValue = s;
					state = 26; // look for (signed) number
				}
				else
				{
					if (   (tokens.at(0).type == tokenStringLiteral)
						|| (tokens.at(0).type == tokenQuotedIdentifier))
					{
						f.defaultisQuoted = true;
					}
					f.defaultValue = s;
					state = 9; // look for column constraint or , or )
				}
				tokens.removeFirst();
				continue;
			case 25: // scan for end of default value expression
				if (s.compare("(") == 0)
				{
					f.defaultValue.append(s);
					++depth;
				}
				else if (s.compare(")") == 0)
				{
					f.defaultValue.append(s);
					if (--depth == 0) { state = 9; }
				}
				else
				{
					f.defaultValue.append(" ");
					if (tokens.at(0).type == tokenQuotedIdentifier)
					{
						f.defaultValue.append(Utils::quote(s));
					}
					else if (tokens.at(0).type == tokenStringLiteral)
					{
						f.defaultValue.append(Utils::literal(s));
					}
					else
					{
						f.defaultValue.append(s);
					}
				}
				tokens.removeFirst();
				continue;
			case 26: // look for (signed) number
				if (tokens.at(0).type == tokenNumeric)
				{
					f.defaultValue.append(s);
					state = 9; // look for column constraint or , or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 27: // look for collation name
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					state = 9; // look for constraint or , or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 28: // look for (foreign) table name
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					f.type = s;
					state = 29; // look for clause or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 29: // look for column list or rest of foreign key clause
				if (s.compare("(") == 0)
				{
					state = 30; // look for column list
				}
				else if (s.compare("ON", Qt::CaseInsensitive) == 0)
				{
					state = 33; // look for DELETE or UPDATE
				}
				else if (s.compare("MATCH", Qt::CaseInsensitive) == 0)
				{
					state = 37; // look for SIMPLE or PARTIAL or FULL
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 38; // look for DEFERRABLE
				}
				else if (s.compare("DEFERRABLE", Qt::CaseInsensitive) == 0)
				{
					state = 39; // look for INITIALLY or next
				}
				else if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0)
				{
					state = 10; // look for column constraint name
				}
				else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0)
				{
					state = 12; // look for KEY
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 18; // look for NULL
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 19; // look for conflict clause or next
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 22; // look for bracketed expression
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 24; // look for default value
				}
				else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 27; // look for collation name
				}
				else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 28; // look for (foreign) table name
				}
				else if (s.compare(",") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 7;
				}
				else if (s.compare(")") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 30: // look for column list
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					state = 31; // look for next column name or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 31: // look for next column name or )
				if (s.compare(",") == 0)
				{
					state = 30; // look for column list
				}
				else if (s.compare(")") == 0)
				{
					state = 32; // look for rest of foreign key clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 32: // look for rest of foreign key clause
				if (s.compare("ON", Qt::CaseInsensitive) == 0)
				{
					state = 33; // look for DELETE or UPDATE
				}
				else if (s.compare("MATCH", Qt::CaseInsensitive) == 0)
				{
					state = 37; // look for SIMPLE or PARTIAL or FULL
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 38; // look for DEFERRABLE
				}
				else if (s.compare("DEFERRABLE", Qt::CaseInsensitive) == 0)
				{
					state = 39; // look for INITIALLY
				}
				else if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0)
				{
					state = 10; // look for column constraint name
				}
				else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0)
				{
					state = 12; // look for KEY
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 18; // look for NULL
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 19; // look for conflict clause or next
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 22; // look for bracketed expression
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 24; // look for default value
				}
				else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 27; // look for collation name
				}
				else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 28; // look for (foreign) table name
				}
				else if (s.compare(",") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 7;
				}
				else if (s.compare(")") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 33: // look for DELETE or UPDATE
				if (s.compare("DELETE", Qt::CaseInsensitive) == 0)
				{
					state = 34; // look for foreign key action
				}
				else if (s.compare("UPDATE", Qt::CaseInsensitive) == 0)
				{
					state = 34; // look for foreign key action
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 34: // look for foreign key action
				if (s.compare("SET", Qt::CaseInsensitive) == 0)
				{
					state = 35; // look for NULL or DEFAULT
				}
				else if (s.compare("CASCADE", Qt::CaseInsensitive) == 0)
				{
					state = 32; // look for rest of foreign key clause
				}
				else if (s.compare("RESTRICT", Qt::CaseInsensitive) == 0)
				{
					state = 32; // look for rest of foreign key clause
				}
				else if (s.compare("NO", Qt::CaseInsensitive) == 0)
				{
					state = 36; // look for ACTION
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 35: // look for NULL or DEFAULT
				if (s.compare("NULL", Qt::CaseInsensitive) == 0)
				{
					state = 32; // look for rest of foreign key clause
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 32; // look for rest of foreign key clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 36: // look for ACTION
				if (s.compare("ACTION", Qt::CaseInsensitive) == 0)
				{
					state = 32; // look for rest of foreign key clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 37: // look for SIMPLE or PARTIAL or FULL
				if (s.compare("SIMPLE", Qt::CaseInsensitive) == 0)
				{
					state = 32; // look for rest of foreign key clause
				}
				else if (s.compare("PARTIAL", Qt::CaseInsensitive) == 0)
				{
					state = 32; // look for rest of foreign key clause
				}
				else if (s.compare("FULL", Qt::CaseInsensitive) == 0)
				{
					state = 32; // look for rest of foreign key clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 38: // look for DEFERRABLE
				if (s.compare("DEFERRABLE", Qt::CaseInsensitive) == 0)
				{
					state = 39; // look for INITIALLY etc
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 39: // look for INITIALLY or next
				if (s.compare("INITIALLY", Qt::CaseInsensitive) == 0)
				{
					state = 40; // look for DEFERRED or IMMEDIATE
				}
				else if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0)
				{
					state = 10; // look for column constraint name
				}
				else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0)
				{
					state = 12; // look for KEY
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 18; // look for NULL
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 19; // look for conflict clause or next
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 22; // look for bracketed expression
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 24; // look for default value
				}
				else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 27; // look for collation name
				}
				else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 28; // look for (foreign) table name
				}
				else if (s.compare(",") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 7;
				}
				else if (s.compare(")") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 40: // look for DEFERRED or IMMEDIATE
				if (s.compare("DEFERRED", Qt::CaseInsensitive) == 0)
				{
					state = 9; // look for column constraint or , or )
				}
				else if (s.compare("IMMEDIATE", Qt::CaseInsensitive) == 0)
				{
					state = 9; // look for column constraint or , or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 41: // look for table constraint name
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					state = 42; // look for rest of table constraint
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 42: // look for rest of table constraint
				if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0)
				{
					state = 43; // look for KEY
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 50; // look for columns and conflict clause
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 59; // look for bracketed expression
				}
				else if (s.compare("FOREIGN", Qt::CaseInsensitive) == 0)
				{
					state = 61; // look for KEY
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 43: // look for KEY
				if (s.compare("KEY", Qt::CaseInsensitive) == 0)
				{
					state = 44; // look for columns and conflict clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 44: // look for columns and conflict clause
				if (s.compare("(") == 0)
				{
					state = 45;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 45: // look for next column in list
				// sqlite doesn't currently support expressions here
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					addToPrimaryKey(s);
					state = 46; // look for COLLATE or ASC/DESC or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 46: // look for COLLATE or ASC/DESC or next
				if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 47; // look for collation name
				}
				else if (s.compare("ASC", Qt::CaseInsensitive) == 0)
				{
					state = 49; // look for , or )
				}
				else if (s.compare("DESC", Qt::CaseInsensitive) == 0)
				{
					state = 49; // look for , or )
				}
				else if (s.compare(",") == 0)
				{
					state = 45; // look for next column in list
				}
				else if (s.compare(")") == 0)
				{
					state = 56; // look for conflict clause or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 47: // look for collation name
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					state = 48; // look for ASC/DESC or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 48: // look for ASC/DESC or next
				if (s.compare("ASC", Qt::CaseInsensitive) == 0)
				{
					state = 49; // look for , or )
				}
				else if (s.compare("DESC", Qt::CaseInsensitive) == 0)
				{
					state = 49; // look for , or )
				}
				else if (s.compare(",") == 0)
				{
					state = 45; // look for next column in list
				}
				else if (s.compare(")") == 0)
				{
					state = 56; // look for conflict clause or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 49: // look for , or )
				if (s.compare(",") == 0)
				{
					state = 45; // look for next column in list
				}
				else if (s.compare(")") == 0)
				{
					state = 56; // look for conflict clause or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 50: // look for columns and conflict clause
				if (s.compare("(") == 0)
				{
					state = 51;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 51: // look for next column in list
				// sqlite doesn't currently support expressions here
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					state = 52; // look for COLLATE or ASC/DESC or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 52: // look for COLLATE or ASC/DESC or next
				if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 53; // look for collation name
				}
				else if (s.compare("ASC", Qt::CaseInsensitive) == 0)
				{
					state = 55; // look for , or )
				}
				else if (s.compare("DESC", Qt::CaseInsensitive) == 0)
				{
					state = 55; // look for , or )
				}
				else if (s.compare(",") == 0)
				{
					state = 51; // look for next column in list
				}
				else if (s.compare(")") == 0)
				{
					state = 56; // look for conflict clause or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 53: // look for collation name
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					state = 54; // look for ASC/DESC or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 54: // look for ASC/DESC or next
				if (s.compare("ASC", Qt::CaseInsensitive) == 0)
				{
					state = 55; // look for , or )
				}
				else if (s.compare("DESC", Qt::CaseInsensitive) == 0)
				{
					state = 55; // look for , or )
				}
				else if (s.compare(",") == 0)
				{
					state = 51; // look for next column in list
				}
				else if (s.compare(")") == 0)
				{
					state = 56; // look for conflict clause or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 55: // look for , or )
				if (s.compare(",") == 0)
				{
					state = 51; // look for next column in list
				}
				else if (s.compare(")") == 0)
				{
					state = 56; // look for conflict clause or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 56: // look for conflict clause or next
				if (s.compare("ON", Qt::CaseInsensitive) == 0)
				{
					state = 57; // look for CONFLICT
				}
				else if (s.compare(",") == 0)
				{
					state = 80; // look for next table constraint
				}
				else if (s.compare(")") == 0)
				{
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 57: // look for CONFLICT
				if (s.compare("CONFLICT", Qt::CaseInsensitive) == 0)
				{
					state = 58; // look for conflict action
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 58: // look for conflict action
				if (s.compare("ROLLBACK", Qt::CaseInsensitive) == 0)
				{
					state = 79; // look for next table constraint or end
				}
				else if (s.compare("ABORT", Qt::CaseInsensitive) == 0)
				{
					state = 79; // look for next table constraint or end
				}
				else if (s.compare("FAIL", Qt::CaseInsensitive) == 0)
				{
					state = 79; // look for next table constraint or end
				}
				else if (s.compare("IGNORE", Qt::CaseInsensitive) == 0)
				{
					state = 79; // look for next table constraint or end
				}
				else if (s.compare("REPLACE", Qt::CaseInsensitive) == 0)
				{
					state = 79; // look for next table constraint or end
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 59: // look for bracketed expression
				if (s.compare("(") == 0)
				{
					state = 60; // look for end of expression
					++depth;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 60: // look for end of expression
				if (s.compare("(") == 0)
				{
					++depth;
				}
				else if (s.compare(")") == 0)
				{
					if (--depth == 0) { state = 79; }
				}
				tokens.removeFirst();
				continue;
			case 61: // look for KEY
				if (s.compare("KEY", Qt::CaseInsensitive) == 0)
				{
					state = 62; // look for column list
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 62: // look for column list
				if (s.compare("(") == 0)
				{
					state = 63; // look for next column in list
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 63: // look for next column in list
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					state = 64; // look for , or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 64: // look for , or )
				if (s.compare(",") == 0)
				{
					state = 63; // look for next column in list
				}
				else if (s.compare(")") == 0)
				{
					state = 65; // look for foreign key clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 65: // look for foreign key clause
				if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 66; // look for table name
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 66: // look for table name
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					state = 67; // look for column list or rest of clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 67: // look for column list or rest of clause
				if (s.compare("(") == 0)
				{
					state = 68; // look for next column in list
				}
				else if (s.compare("ON", Qt::CaseInsensitive) == 0)
				{
					state = 71; // look for DELETE or UPDATE
				}
				else if (s.compare("MATCH", Qt::CaseInsensitive) == 0)
				{
					state = 75; // look for SIMPLE or PARTIAL or FULL
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 76; // look for DEFERRABLE
				}
				else if (s.compare("DEFERRABLE", Qt::CaseInsensitive) == 0)
				{
					state = 77; // look for INITIALLY
				}
				else if (s.compare(",") == 0)
				{
					state = 80; // look for next table constraint
				}
				else if (s.compare(")") == 0)
				{
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 68: // look for next column in list
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					state = 69; // look for , or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 69:
				if (s.compare(",") == 0)
				{
					state = 68; // look for next column in list
				}
				else if (s.compare(")") == 0)
				{
					state = 70; // look for rest of foreign key clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 70: // look for rest of foreign key clause
				if (s.compare("ON", Qt::CaseInsensitive) == 0)
				{
					state = 71; // look for DELETE or UPDATE
				}
				else if (s.compare("MATCH", Qt::CaseInsensitive) == 0)
				{
					state = 75; // look for SIMPLE or PARTIAL or FULL
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 76; // look for DEFERRABLE
				}
				else if (s.compare("DEFERRABLE", Qt::CaseInsensitive) == 0)
				{
					state = 77; // look for INITIALLY
				}
				else if (s.compare(",") == 0)
				{
					state = 80; // look for next table constraint
				}
				else if (s.compare(")") == 0)
				{
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 71: // look for DELETE or UPDATE
				if (s.compare("DELETE", Qt::CaseInsensitive) == 0)
				{
					state = 72; // look for foreign key action
				}
				else if (s.compare("UPDATE", Qt::CaseInsensitive) == 0)
				{
					state = 72; // look for foreign key action
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 72: // look for foreign key action
				if (s.compare("SET", Qt::CaseInsensitive) == 0)
				{
					state = 73; // look for NULL or DEFAULT
				}
				else if (s.compare("CASCADE", Qt::CaseInsensitive) == 0)
				{
					state = 70; // look for rest of foreign key clause
				}
				else if (s.compare("RESTRICT", Qt::CaseInsensitive) == 0)
				{
					state = 70; // look for rest of foreign key clause
				}
				else if (s.compare("NO", Qt::CaseInsensitive) == 0)
				{
					state = 74; // look for ACTION
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 73: // look for NULL or DEFAULT
				if (s.compare("NULL", Qt::CaseInsensitive) == 0)
				{
					state = 70; // look for rest of foreign key clause
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 70; // look for rest of foreign key clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 74: // look for ACTION
				if (s.compare("ACTION", Qt::CaseInsensitive) == 0)
				{
					state = 70; // look for rest of foreign key clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 75: // look for SIMPLE or PARTIAL or FULL
				if (s.compare("SIMPLE", Qt::CaseInsensitive) == 0)
				{
					state = 70; // look for rest of foreign key clause
				}
				else if (s.compare("PARTIAL", Qt::CaseInsensitive) == 0)
				{
					state = 70; // look for rest of foreign key clause
				}
				else if (s.compare("FULL", Qt::CaseInsensitive) == 0)
				{
					state = 70; // look for rest of foreign key clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 76: // look for DEFERRABLE
				if (s.compare("DEFERRABLE", Qt::CaseInsensitive) == 0)
				{
					state = 77; // look for INITIALLY etc
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 77: // look for INITIALLY or next
				if (s.compare("INITIALLY", Qt::CaseInsensitive) == 0)
				{
					state = 78; // look for DEFERRED or IMMEDIATE
				}
				else if (s.compare(",") == 0)
				{
					state = 80; // look for next table constraint
				}
				else if (s.compare(")") == 0)
				{
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 78: // look for DEFERRED or IMMEDIATE
				if (s.compare("DEFERRED", Qt::CaseInsensitive) == 0)
				{
					state = 79; // look for next table constraint or end
				}
				else if (s.compare("IMMEDIATE", Qt::CaseInsensitive) == 0)
				{
					state = 79; // look for next table constraint or end
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 79: // look for next table constraint or end
				if (s.compare(",") == 0)
				{
					state = 80; // look for next table constraint
				}
				else if (s.compare(")") == 0)
				{
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 80: // look for next table constraint
				if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0)
				{
					state = 41; // look for table constraint name
				}
				else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0)
				{
					state = 43; // look for KEY
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 50; // look for columns and conflict clause
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 59; // look for bracketed expression
				}
				else if (s.compare("FOREIGN", Qt::CaseInsensitive) == 0)
				{
					state = 61; // look for KEY
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 81: // check for WITHOUT ROWID or rubbish at end
				m_isValid = false;
				if (s.compare("WITHOUT", Qt::CaseInsensitive) == 0)
				{
					state = 82; // seen WITHOUT
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 82: // seen WITHOUT
				if (s.compare("ROWID", Qt::CaseInsensitive) == 0)
				{
					state = 83; // seen ROWID
					m_isValid = true;
					m_hasRowid = false;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 83: // seen ROWID, anything after it is an error
				m_isValid = false;
				break;
		}
		break;
	}
}

QString SqlParser::defaultToken(FieldInfo &f)
{
	QString result(f.defaultValue);
	if (result.isEmpty()) { return QString(); }
	else if (f.defaultisQuoted)
	{
		return Utils::literal(result);
	}
	else {return result; }
}
