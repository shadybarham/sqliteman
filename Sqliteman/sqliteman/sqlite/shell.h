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

typedef sqlite3_int64 i64;
typedef sqlite3_uint64 u64;
typedef unsigned char u8;

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
** A single constraint. Equivalent to either "col = ?" or "col < ?" (or
** any other type of single-ended range constraint on a column).
**
** pLink:
**   Used to temporarily link IdxConstraint objects into lists while
**   creating candidate indexes.
*/
struct IdxConstraint {
  char *zColl;                    /* Collation sequence */
  int bRange;                     /* True for range, false for eq */
  int iCol;                       /* Constrained table column */
  int bFlag;                      /* Used by idxFindCompatible() */
  int bDesc;                      /* True if ORDER BY <expr> DESC */
  IdxConstraint *pNext;           /* Next constraint in pEq or pRange list */
  IdxConstraint *pLink;           /* See above */
};
typedef struct IdxConstraint IdxConstraint;

/*
** Information regarding a single database table. Extracted from 
** "PRAGMA table_info" by function idxGetTableInfo().
*/
struct IdxColumn {
  char *zName;
  char *zColl;
  int iPk;
};
typedef struct IdxColumn IdxColumn;

struct IdxTable {
  int nCol;
  char *zName;                    /* Table name */
  IdxColumn *aCol;
  IdxTable *pNext;                /* Next table in linked list of all tables */
};
typedef struct IdxTable IdxTable;

/*
** A single scan of a single table.
*/
struct IdxScan {
  IdxTable *pTab;                 /* Associated table object */
  int iDb;                        /* Database containing table zTable */
  i64 covering;                   /* Mask of columns required for cov. index */
  IdxConstraint *pOrder;          /* ORDER BY columns */
  IdxConstraint *pEq;             /* List of == constraints */
  IdxConstraint *pRange;          /* List of < constraints */
  IdxScan *pNextScan;             /* Next IdxScan object for same analysis */
};
typedef struct IdxScan IdxScan;

/*
** An object of the following type is created for each unique table/write-op
** seen. The objects are stored in a singly-linked list beginning at
** sqlite3expert.pWrite.
*/
struct IdxWrite {
  IdxTable *pTab;
  int eOp;                        /* SQLITE_UPDATE, DELETE or INSERT */
  IdxWrite *pNext;
};
typedef struct IdxWrite IdxWrite;

/*
** Each statement being analyzed is represented by an instance of this
** structure.
*/
struct IdxStatement {
  int iId;                        /* Statement number */
  char *zSql;                     /* SQL statement */
  char *zIdx;                     /* Indexes */
  char *zEQP;                     /* Plan */
  IdxStatement *pNext;
};
typedef struct IdxStatement IdxStatement;

/*
** A hash table for storing strings. With space for a payload string
** with each entry. Methods are:
**
**   idxHashInit()
**   idxHashClear()
**   idxHashAdd()
**   idxHashSearch()
*/
#define IDX_HASH_SIZE 1023
typedef struct IdxHashEntry IdxHashEntry;
typedef struct IdxHash IdxHash;
struct IdxHashEntry {
  char *zKey;                     /* nul-terminated key */
  char *zVal;                     /* nul-terminated value string */
  char *zVal2;                    /* nul-terminated value string 2 */
  IdxHashEntry *pHashNext;        /* Next entry in same hash bucket */
  IdxHashEntry *pNext;            /* Next entry in hash */
};
struct IdxHash {
  IdxHashEntry *pFirst;
  IdxHashEntry *aHash[IDX_HASH_SIZE];
};

/*
** sqlite3expert object.
*/
struct sqlite3expert {
  int iSample;                    /* Percentage of tables to sample for stat1 */
  sqlite3 *db;                    /* User database */
  sqlite3 *dbm;                   /* In-memory db for this analysis */
  sqlite3 *dbv;                   /* Vtab schema for this analysis */
  IdxTable *pTable;               /* List of all IdxTable objects */
  IdxScan *pScan;                 /* List of scan objects */
  IdxWrite *pWrite;               /* List of write objects */
  IdxStatement *pStatement;       /* List of IdxStatement objects */
  int bRun;                       /* True once analysis has run */
  char **pzErrmsg;
  int rc;                         /* Error code from whereinfo hook */
  IdxHash hIdx;                   /* Hash containing all candidate indexes */
  char *zCandidates;              /* For EXPERT_REPORT_CANDIDATES */
};

typedef struct ExpertInfo ExpertInfo;
struct ExpertInfo {
  sqlite3expert *pExpert;
  int bVerbose;
};
/* A single line in the EQP output */
typedef struct EQPGraphRow EQPGraphRow;
struct EQPGraphRow {
  int iEqpId;           /* ID for this row */
  int iParentId;        /* ID of the parent row */
  EQPGraphRow *pNext;   /* Next row in sequence */
  char zText[1];        /* Text to display for this row */
};

/* All EQP output is collected into an instance of the following */
typedef struct EQPGraph EQPGraph;
struct EQPGraph {
  EQPGraphRow *pRow;    /* Linked list of all rows of the EQP output */
  EQPGraphRow *pLast;   /* Last element of the pRow list */
  char zPrefix[100];    /* Graph prefix */
};

/*
** State information about the database connection is contained in an
** instance of the following structure.
*/
typedef struct ShellState ShellState;
struct ShellState {
  sqlite3 *db;           /* The database */
  u8 autoExplain;        /* Automatically turn on .explain mode */
  u8 autoEQP;            /* Run EXPLAIN QUERY PLAN prior to seach SQL stmt */
  u8 autoEQPtest;        /* autoEQP is in test mode */
  u8 autoEQPtrace;       /* autoEQP is in trace mode */
  u8 statsOn;            /* True to display memory stats before each finalize */
  u8 scanstatsOn;        /* True to display scan stats before each finalize */
  u8 openMode;           /* SHELL_OPEN_NORMAL, _APPENDVFS, or _ZIPFILE */
  u8 doXdgOpen;          /* Invoke start/open/xdg-open in output_reset() */
  u8 nEqpLevel;          /* Depth of the EQP output graph */
  u8 eTraceType;         /* SHELL_TRACE_* value for type of trace */
  unsigned mEqpLines;    /* Mask of veritical lines in the EQP output graph */
  int outCount;          /* Revert to stdout when reaching zero */
  int cnt;               /* Number of records displayed so far */
  int lineno;            /* Line number of last line read from in */
  FILE *in;              /* Read commands from this stream */
  FILE *out;             /* Write results here */
  FILE *traceOut;        /* Output for sqlite3_trace() */
  int nErr;              /* Number of errors seen */
  int mode;              /* An output mode setting */
  int modePrior;         /* Saved mode */
  int cMode;             /* temporary output mode for the current query */
  int normalMode;        /* Output mode before ".explain on" */
  int writableSchema;    /* True if PRAGMA writable_schema=ON */
  int showHeader;        /* True to show column names in List or Column mode */
  int nCheck;            /* Number of ".check" commands run */
  unsigned nProgress;    /* Number of progress callbacks encountered */
  unsigned mxProgress;   /* Maximum progress callbacks before failing */
  unsigned flgProgress;  /* Flags for the progress callback */
  unsigned shellFlgs;    /* Various flags */
  sqlite3_int64 szMax;   /* --maxsize argument to .open */
  char *zDestTable;      /* Name of destination table when MODE_Insert */
  char *zTempFile;       /* Temporary file that might need deleting */
  char zTestcase[30];    /* Name of current test case */
  char colSeparator[20]; /* Column separator character for several modes */
  char rowSeparator[20]; /* Row separator character for MODE_Ascii */
  char colSepPrior[20];  /* Saved column separator */
  char rowSepPrior[20];  /* Saved row separator */
  int colWidth[100];     /* Requested width of each column when in column mode*/
  int actualWidth[100];  /* Actual width of each column */
  char nullValue[20];    /* The text to print when a NULL comes back from
                         ** the database */
  char outfile[FILENAME_MAX]; /* Filename for *out */
  const char *zDbFilename;    /* name of the database file */
  char *zFreeOnClose;         /* Filename to free when closing */
  const char *zVfs;           /* Name of VFS to use */
  sqlite3_stmt *pStmt;   /* Current statement if any. */
  FILE *pLog;            /* Write log output here */
  int *aiIndent;         /* Array of indents used in MODE_Explain */
  int nIndent;           /* Size of array aiIndent[] */
  int iIndent;           /* Index of current op in aiIndent[] */
  EQPGraph sGraph;       /* Information for the graphical EXPLAIN QUERY PLAN */
#if defined(SQLITE_ENABLE_SESSION)
  int nSession;             /* Number of active sessions */
  OpenSession aSession[4];  /* Array of sessions.  [0] is in focus. */
#endif
  ExpertInfo expert;        /* Valid if previous command was ".expert OPT..." */
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
