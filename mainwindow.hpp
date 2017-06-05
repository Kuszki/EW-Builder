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

#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>

#include "databasedriver.hpp"
#include "connectdialog.hpp"
#include "updatewidget.hpp"
#include "aboutdialog.hpp"

namespace Ui
{
	class MainWindow;
}

class MainWindow : public QMainWindow
{

		Q_OBJECT

	public: enum STATUS
	{
		CONNECTED,
		DISCONNECTED,
		BUSY,
		DONE
	};

	private:

		QHash<QString, QHash<int, QString>> lineLayers;
		QHash<QString, QHash<int, QString>> pointLayers;
		QHash<QString, QHash<int, QString>> textLayers;

		QList<DatabaseDriver::TABLE> classesData;

		Ui::MainWindow* ui;

		DatabaseDriver* Driver;
		AboutDialog* About;

		QThread Thread;

		unsigned commonCount = 0;

	private:

		void lockUi(STATUS Status);

	public:

		explicit MainWindow(QWidget* Parent = nullptr);
		virtual ~MainWindow(void) override;

	private slots:

		void connectActionClicked(void);
		void proceedActionClicked(void);

		void databaseConnected(const QList<DatabaseDriver::TABLE>& Classes, unsigned Common,
						   const QHash<QString, QHash<int, QString>>& Lines,
						   const QHash<QString, QHash<int, QString>>& Points,
						   const QHash<QString, QHash<int, QString>>& Texts);

		void databaseDisconnected(void);

		void classIndexChanged(int Index);

		void databaseLogin(bool OK);

		void loginAttempt(void);

	signals:

		void onExecRequest(const QHash<int, QVariant>&,
					    const QString&, int, int, int);

};

#endif // MAINWINDOW_HPP
