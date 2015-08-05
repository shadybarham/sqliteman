/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef ALTERVIEWDIALOG_H
#define ALTERVIEWDIALOG_H

#include <qwidget.h>

#include "litemanwindow.h"
#include "ui_createviewdialog.h"


/*! \brief GUI for view altering
\author Petr Vanek <petr@scribus.info>
*/
class AlterViewDialog : public QDialog
{
	Q_OBJECT

	public:
		AlterViewDialog(const QString & name,
						const QString & schema, LiteManWindow * parent = 0);
		~AlterViewDialog(){};

		bool update;
		void setText(const QString & text) { ui.sqlEdit->setText(text); };

	private:
		Ui::CreateViewDialog ui;
		
		// We ought to be able use use parent() for this, but for some reason
		// qobject_cast<LiteManWindow*>(parent()) doesn't work
		LiteManWindow * creator;

	private slots:
		void createButton_clicked();
};

#endif
