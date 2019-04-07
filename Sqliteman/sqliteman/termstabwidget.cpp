/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QComboBox>
#include <QLineEdit>

#include "termstabwidget.h"
#include "utils.h"

TermsTabWidget::TermsTabWidget(QWidget * parent): QWidget(parent)
{
	setupUi(this);

	termsTable->setColumnCount(3);
	termsTable->horizontalHeader()->hide();
	termsTable->verticalHeader()->hide();

    // needed because the LineEdit's frame has vanished
	termsTable->setShowGrid(true);

	connect(termMoreButton, SIGNAL(clicked()),
		this, SLOT(moreTerms()));
	connect(termLessButton, SIGNAL(clicked()),
		this, SLOT(lessTerms()));
}

void TermsTabWidget::paintEvent(QPaintEvent * event)
{
    // force at least one term because it isn't much use without one
    // for some reason calling moreTerms() in the constructor doesn't work
    if (termsTable->rowCount() == 0) { moreTerms(); }
	Utils::setColumnWidths(termsTable);
	QWidget::paintEvent(event);
}

void TermsTabWidget::moreTerms()
{
	int i = termsTable->rowCount();
	termsTable->setRowCount(i + 1);
	QComboBox * fields = new QComboBox();
	fields->addItems(m_columnList);
	termsTable->setCellWidget(i, 0, fields);
	QComboBox * relations = new QComboBox();
	relations->addItems(QStringList() << tr("Contains") << tr("Doesn't contain")
									  << tr("Starts with")
									  << tr("Equals") << tr("Not equals")
									  << tr("Bigger than") << tr("Smaller than")
									  << tr("Is null") << tr("Is not null"));
	termsTable->setCellWidget(i, 1, relations);
	connect(relations, SIGNAL(currentIndexChanged(const QString &)),
			this, SLOT(relationsIndexChanged(const QString &)));
	QLineEdit * value = new QLineEdit();
	termsTable->setCellWidget(i, 2, value);
	termsTable->resizeColumnsToContents();
	termLessButton->setEnabled(i > 0);
	update();
}

void TermsTabWidget::lessTerms()
{
	int i = termsTable->rowCount() - 1;
	termsTable->removeRow(i);
	termsTable->resizeColumnsToContents();
	if (i == 1) { termLessButton->setEnabled(false); }
	update();
}

void TermsTabWidget::relationsIndexChanged(const QString &)
{
	QComboBox * relations = qobject_cast<QComboBox *>(sender());
	for (int i = 0; i < termsTable->rowCount(); ++i)
	{
		if (relations == termsTable->cellWidget(i, 1))
		{
			switch (relations->currentIndex())
			{
				case 0: // Contains
				case 1: // Doesn't contain
				case 2: // Starts with
				case 3: // Equals
				case 4: // Not equals
				case 5: // Bigger than
				case 6: // Smaller than
					if (!(termsTable->cellWidget(i, 2)))
					{
						termsTable->setCellWidget(i, 2, new QLineEdit());
					}
					return;

				case 7: // is null
				case 8: // is not null
					termsTable->removeCellWidget(i, 2);
					return;
			}
		}
	}
	update();
}
