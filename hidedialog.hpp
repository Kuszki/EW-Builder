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

#ifndef HIDEDIALOG_HPP
#define HIDEDIALOG_HPP

#include <QCheckBox>
#include <QDialog>
#include <QHash>
#include <QSet>

namespace Ui
{
	class HideDialog;
}

class HideDialog : public QDialog
{

		Q_OBJECT

	private:

		Ui::HideDialog* ui;

	public:

		explicit HideDialog(const QHash<int, QString>& Hides, QWidget* Parent = nullptr);
		virtual ~HideDialog(void) override;

		QSet<int> getSelectedFields(void) const;

	public slots:

		virtual void accept(void) override;

		void setFields(const QHash<int, QString>& List);

		void setUnchecked(void);

	signals:

		void onHideRequest(const QSet<int>&, bool);

};

#endif // HIDEDIALOG_HPP
