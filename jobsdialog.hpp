/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  EW-Mapa object creator                                                 *
 *  Copyright (C) 2016  Łukasz "Kuszki" Dróżdż  l.drozdz@openmailbox.org   *
 *                                                                         *
 *  This program is free software: you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the  Free Software Foundation, either  version 3 of the  License, or   *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This  program  is  distributed  in the hope  that it will be useful,   *
 *  but WITHOUT ANY  WARRANTY;  without  even  the  implied  warranty of   *
 *  MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the   *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have  received a copy  of the  GNU General Public License   *
 *  along with this program. If not, see http://www.gnu.org/licenses/.     *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef JOBSDIALOG_HPP
#define JOBSDIALOG_HPP

#include <QDialogButtonBox>
#include <QPushButton>
#include <QFileDialog>
#include <QDialog>
#include <QSet>

namespace Ui
{
	class JobsDialog;
}

class JobsDialog : public QDialog
{

		Q_OBJECT

	private:

		Ui::JobsDialog* ui;

	public:

		explicit JobsDialog(QWidget* Parent = nullptr);
		virtual ~JobsDialog(void) override;

	public slots:

		virtual void accept(void) override;

	private slots:

		void openButtonClicked(void);

		void dialogDataChanged(void);

	signals:

		void onFitRequest(const QString&,
					   const QString&,
					   int, int, int);

};

#endif // JOBSDIALOG_HPP
