/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.

This file written by and copyright © 2015 Richard P. Parkins, M. A., and released
under the GPL.

It contains static declarations used in regular expressions to match SQL
syntax. Note that the regular expressions used by Qt cannot define a regexp
which matches parentheses matched to unlimited depth. We arbitrarily use a limit
of 10 for SQL expression matching. The sqlite syntax decoder appears to use
case-insensitive matching, so all regexps must be set to case insensitive. We
capitalise keywords for clarity.

There is no matcher for bind parameters because there is no context in which we
ever want to match one: we only match aginst fully resolved SQL statements.

We match some sequences that are not valid, but we never expect to see one since
anything we look at has already been parsed by sqlite.
*/

// This matches an SQL string literal (or an identifier)
static const QString sqlName =
"(?:"
	"(?:"
		"(?:"
			"'[^']*'"
		")+"
	")|(?:"
		"(?:"
			"\"[^\"]*\""
		")+"
	")|(?:"
		"[0-9a-z_]+"
	")"
")";

// This matches an SQL numeric literal
static const QString sqlNumber =
"(?:"
	"(?:"
		"(?:"
			"(?:"
				"[0-9]+(?:"
					"\\.[0-9]*"
				")?"
			")|(?:"
				"\\.[0-9]+"
			")"
		")(?:"
			"E[+-]?[0-9]+"
		")?"
	")"
")|(?:"
	"0x[0-9a-f]+"
")";

// This matches an SQL blob literal
static const QString sqlBlob = "(?:x'[0-9a-f]+')";

// This matches a terminal symbol in the SQL parse tree
static const QString sqlTerminal =
QString("(?:")+
	sqlNumber +
	"|" + sqlName + 
	"|" + sqlBlob + 
")";

// This matches any of the operators which can be in an SQL expression
// We allow consecutive operators or consecutive terminals. This isn't valid SQL
// syntax, but avoids difficulties matching both << and <, etc, and allows us not
// to special case things like GLOB.
static const QString sqlOperator = "(?:[\\.\\*/%\\+-<>&=!])";

// 10 levels of expression nesting, see above
static const QString sqlExp0 =
QString("(?:")+
	"(?:" +
		"(?:" +
			"\\s*" + sqlTerminal +
		")|(?:" +
			"\\s*" + sqlOperator +
		")"
	")+" +
")";
static const QString sqlExp1 =
QString("(?:") +
	"(?:" +
		sqlExp0 + "|" +
		"(?:" +
			"\\s*\\((?:" +
				sqlExp0 + "|(?:\\s*,)" +
			")*\\s*\\)" +
		")" +
	")+" +
")";
static const QString sqlExp2 =
QString("(?:") +
	"(?:" +
		sqlExp1 + "|" +
		"(?:" +
			"\\s*\\((?:" +
				sqlExp1 + "|(?:\\s*,)" +
			")*\\s*\\)" +
		")" +
	")+" +
")";
static const QString sqlExp3 =
QString("(?:") +
	"(?:" +
		sqlExp2 + "|" +
		"(?:" +
			"\\s*\\((?:" +
				sqlExp2 + "|(?:\\s*,)" +
			")*\\s*\\)" +
		")" +
	")+" +
")";
static const QString sqlExp4 =
QString("(?:") +
	"(?:" +
		sqlExp3 + "|" +
		"(?:" +
			"\\s*\\((?:" +
				sqlExp3 + "|(?:\\s*,)" +
			")*\\s*\\)" +
		")" +
	")+" +
")";
static const QString sqlExp5 =
QString("(?:") +
	"(?:" +
		sqlExp4 + "|" +
		"(?:" +
			"\\s*\\((?:" +
				sqlExp4 + "|(?:\\s*,)" +
			")*\\s*\\)" +
		")" +
	")+" +
")";
static const QString sqlExp6 =
QString("(?:") +
	"(?:" +
		sqlExp5 + "|" +
		"(?:" +
			"\\s*\\((?:" +
				sqlExp5 + "|(?:\\s*,)" +
			")*\\s*\\)" +
		")" +
	")+" +
")";
static const QString sqlExp7 =
QString("(?:") +
	"(?:" +
		sqlExp6 + "|" +
		"(?:" +
			"\\s*\\((?:" +
				sqlExp6 + "|(?:\\s*,)" +
			")*\\s*\\)" +
		")" +
	")+" +
")";
static const QString sqlExp8 =
QString("(?:") +
	"(?:" +
		sqlExp7 + "|" +
		"(?:" +
			"\\s*\\((?:" +
				sqlExp7 + "|(?:\\s*,)" +
			")*\\s*\\)" +
		")" +
	")+" +
")";
static const QString sqlExp9 =
QString("(?:") +
	"(?:" +
		sqlExp8 + "|" +
		"(?:" +
			"\\s*\\((?:" +
				sqlExp8 + "|(?:\\s*,)" +
			")*\\s*\\)" +
		")" +
	")+" +
")";
static const QString sqlExpression =
QString("(?:") +
	"(?:" +
		sqlExp9 + "|" +
		"(?:" +
			"\\s*\\((?:" +
				sqlExp9 + "|(?:\\s*,)" +
			")*\\s*\\)" +
		")" +
	")+" +
")";

// This matches a CREATE statement in the schema (no AS option)
static const QString sqlCreate =
QString("^\\s*CREATE")+
"(?:" +
	"(?:\\s*TEMP(?:ORARY)?)?" + 
")" + 
"\\s*TABLE" +
	"(?:" +
		"(?:\\s*IF\\s*NOT\\s*EXISTS)?" + 
	")" +
"\\s*" + sqlName +
"(?:(?:\\s*\\.\\s*" + sqlName + ")?)" +
"\\s*\\((" + 
	sqlExpression + "(?:" +
		"(?:\\s*," + sqlExpression + ")*" +
	")" +
")\\s*\\)" +
"(?:(?:\\s*WITHOUT\\s+ROWID)?)\\s*$";

// Rest of column list
static const QString sqlTail =
QString("(?:\\s*")+
	"(?:" +
		"(?:" +
			"(?:" +
				",\\s*CONSTRAINT" + sqlExpression +
			")*\\s*$" +
		")|(?:,"+
			"(?!\\s*CONSTRAINT)" +
			"(" +
				sqlExpression + "(?:" +
					"(?:\\s*," + sqlExpression + ")*" +
				")"+
			")\\s*$" +
		")" +
	")" +
")";

// These split a field name and a type 
static const QString sqlField =
QString("^\\s*(")+ sqlName + ")(?:" +
	"(?:" +
		"(?:" +
			"()\\s*CONSTRAINT" + sqlExpression +
		")|(?:" +
			"(?!\\s*CONSTRAINT)" +
			"(?:" +
				"(?:" +
					"\\s*(" + sqlName + ")" +
					"(?:(?:" + sqlExpression + ")?)" +
				")?" +
			")" +
		")" +
	")" + sqlTail +
")";
