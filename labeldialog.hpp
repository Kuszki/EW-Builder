/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  EW-Mapa object creator                                                 *
 *  Copyright (C) 2017  Łukasz "Kuszki" Dróżdż  l.drozdz@o2.pl             *
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

#ifndef LABELDIALOG_HPP
#define LABELDIALOG_HPP

#include <QDialog>
#include <QMap>

namespace Ui
{
	class LabelDialog;
}

class LabelDialog : public QDialog
{

		Q_OBJECT

	private:

		QHash<QString, QHash<int, QString>> textLayers;
		QMap<QString, QString> classCodes;

		Ui::LabelDialog* ui;

	public:

		explicit LabelDialog(QMap<QString, QString> Classes,
						 QHash<QString, QHash<int, QString>> Text,
						 QWidget* Parent = nullptr);
		virtual ~LabelDialog(void) override;

	private slots:

		void currentClassChanged(int Index);

	public slots:

		virtual void accept(void) override;

		void setParameters(QMap<QString, QString> Classes,
					    QHash<QString, QHash<int, QString>> Text);

	signals:

		void onLabelsRequest(const QString&,
						 const QString&,
						 int, int,
						 double, double,
						 bool);

};

#endif // LABELDIALOG_HPP
