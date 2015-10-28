/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.

This file is only a header-wrapper for required functions from shell.c.
*/

#ifndef SHELL_H
#define SHELL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sqlite3.h"


/*
** Shell output mode information from before ".explain on", 
** saved so that it can be restored by ".explain off"
*/
typedef struct SavedModeInfo SavedModeInfo;
struct SavedModeInfo {
  int valid;          /* Is there legit data in here? */
  int mode;           /* Mode prior to ".explain on" */
  int showHeader;     /* The ".header" setting prior to ".explain on" */
  int colWidth[100];  /* Column widths prior to ".explain on" */
};
/*
** State information about the database connection is contained in an
** instance of the following structure.
*/
typedef struct ShellState ShellState;
struct ShellState {
  sqlite3 *db;           /* The database */
  int echoOn;            /* True to echo input commands */
  int autoEQP;           /* Run EXPLAIN QUERY PLAN prior to seach SQL stmt */
  int statsOn;           /* True to display memory stats before each finalize */
  int scanstatsOn;       /* True to display scan stats before each finalize */
  int backslashOn;       /* Resolve C-style \x escapes in SQL input text */
  int outCount;          /* Revert to stdout when reaching zero */
  int cnt;               /* Number of records displayed so far */
  FILE *out;             /* Write results here */
  FILE *traceOut;        /* Output for sqlite3_trace() */
  int nErr;              /* Number of errors seen */
  int mode;              /* An output mode setting */
  int writableSchema;    /* True if PRAGMA writable_schema=ON */
  int showHeader;        /* True to show column names in List or Column mode */
  unsigned shellFlgs;    /* Various flags */
  char *zDestTable;      /* Name of destination table when MODE_Insert */
  char colSeparator[20]; /* Column separator character for several modes */
  char rowSeparator[20]; /* Row separator character for MODE_Ascii */
  int colWidth[100];     /* Requested width of each column when in column mode*/
  int actualWidth[100];  /* Actual width of each column */
  char nullValue[20];    /* The text to print when a NULL comes back from
                         ** the database */
  SavedModeInfo normalMode;/* Holds the mode just before .explain ON */
  char outfile[FILENAME_MAX]; /* Filename for *out */
  const char *zDbFilename;    /* name of the database file */
  char *zFreeOnClose;         /* Filename to free when closing */
  const char *zVfs;           /* Name of VFS to use */
  sqlite3_stmt *pStmt;   /* Current statement if any. */
  FILE *pLog;            /* Write log output here */
  int *aiIndent;         /* Array of indents used in MODE_Explain */
  int nIndent;           /* Size of array aiIndent[] */
  int iIndent;           /* Index of current op in aiIndent[] */
};

/*
** Execute a query statement that has a single result column.  Print
** that result column on a line by itself with a semicolon terminator.
**
** This is used, for example, to show the schema of the database by
** querying the SQLITE_MASTER table.
*/
int run_table_dump_query(
  ShellState *p,           /* Query context */
  const char *zSelect,     /* SELECT statement to extract content */
  const char *zFirstRow    /* Print before first row, if not NULL */
);

/*
** Run zQuery.  Use dump_callback() as the callback routine so that
** the contents of the query are output as SQL statements.
**
** If we get a SQLITE_CORRUPT error, rerun the query after appending
** "ORDER BY rowid DESC" to the end.
*/
int run_schema_dump_query(
  ShellState *p, 
  const char *zQuery
);

#ifdef __cplusplus
}  /* End of the 'extern "C"' block */
#endif

#endif
