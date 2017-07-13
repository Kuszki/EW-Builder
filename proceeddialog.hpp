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

#ifndef PROCEEDDIALOG_HPP
#define PROCEEDDIALOG_HPP

#include <QDialogButtonBox>
#include <QPushButton>
#include <QDialog>

namespace Ui
{
	class ProceedDialog;
}

class ProceedDialog : public QDialog
{

		Q_OBJECT

	private:

		Ui::ProceedDialog* ui;

	public:

		explicit ProceedDialog(QWidget* Parent = nullptr);
		virtual ~ProceedDialog(void) override;

	private slots:

		void symbolTextChanged(const QString& Text);

		void pointStrategyChanged(int Index);

	public slots:

		virtual void accept(void) override;

	signals:

		void onProceedRequest(double, bool, bool, int, const QString&);

};

#endif // PROCEEDDIALOG_HPP
