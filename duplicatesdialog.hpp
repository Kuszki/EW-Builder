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

#ifndef DUPLICATESDIALOG_HPP
#define DUPLICATESDIALOG_HPP

#include <QDialog>

#include "databasedriver.hpp"

namespace Ui
{
	class DuplicatesDialog;
}

class DuplicatesDialog : public QDialog
{

		Q_OBJECT

	private:

		QList<DatabaseDriver::LAYER> lineLayers;
		QList<DatabaseDriver::LAYER> textLayers;

		Ui::DuplicatesDialog* ui;

	public:

		explicit DuplicatesDialog(const QList<DatabaseDriver::LAYER> Lines,
							 const QList<DatabaseDriver::LAYER> Texts,
							 QWidget* Parent = nullptr);
		virtual ~DuplicatesDialog(void) override;

	private slots:

		void actionIndexChanged(int Index);
		void strategyIndexChanged(int Index);
		void typeIndexChanged(int Index);
		void layerIndexChanged(int Index);

	public slots:

		virtual void accept(void) override;

		void setParameters(const QList<DatabaseDriver::LAYER> Lines,
					    const QList<DatabaseDriver::LAYER> Texts);

	signals:

		void onRemoveRequest(int, int, int, int, int, int, double);

};

#endif // DUPLICATESDIALOG_HPP
