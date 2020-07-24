/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  EW-Mapa object creator                                                 *
 *  Copyright (C) 2016  Łukasz "Kuszki" Dróżdż  l.drozdz@o2.pl             *
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

#ifndef FITDIALOG_HPP
#define FITDIALOG_HPP

#include <QDialogButtonBox>
#include <QPushButton>
#include <QFileDialog>
#include <QDialog>
#include <QSet>

namespace Ui
{
	class FitDialog;
}

class FitDialog : public QDialog
{

		Q_OBJECT

	private:

		Ui::FitDialog* ui;

	public:

		explicit FitDialog(QWidget* Parent = nullptr);
		virtual ~FitDialog(void) override;

	public slots:

		virtual void accept(void) override;

	private slots:

		void actionIndexChanged(int Index);

		void fitParametersChanged(void);

		void openButtonClicked(void);

	signals:

		void onFitRequest(const QString&, int, int, double, bool);

};

#endif // FITDIALOG_HPP
