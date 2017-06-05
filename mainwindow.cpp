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

#include "mainwindow.hpp"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *Parent)
: QMainWindow(Parent), ui(new Ui::MainWindow)
{
	ui->setupUi(this); lockUi(DISCONNECTED);

	Driver = new DatabaseDriver(nullptr);

	QSettings Settings("EW-Database");
	About = new AboutDialog(this);

	ui->Progress->hide();
	Driver->moveToThread(&Thread);
	Thread.start();

	ui->valuesLayout->setAlignment(Qt::AlignTop);

	Settings.beginGroup("Builder");
	restoreGeometry(Settings.value("geometry").toByteArray());
	restoreState(Settings.value("state").toByteArray());
	Settings.endGroup();

	connect(Driver, &DatabaseDriver::onBeginProgress, ui->Progress, &QProgressBar::show);
	connect(Driver, &DatabaseDriver::onSetupProgress, ui->Progress, &QProgressBar::setRange);
	connect(Driver, &DatabaseDriver::onUpdateProgress, ui->Progress, &QProgressBar::setValue);
	connect(Driver, &DatabaseDriver::onEndProgress, ui->Progress, &QProgressBar::hide);

	connect(Driver, &DatabaseDriver::onConnect, this, &MainWindow::databaseConnected);
	connect(Driver, &DatabaseDriver::onDisconnect, this, &MainWindow::databaseDisconnected);
	connect(Driver, &DatabaseDriver::onLogin, this, &MainWindow::databaseLogin);

	connect(ui->actionConnect, &QAction::triggered, this, &MainWindow::connectActionClicked);
	connect(ui->actionDisconnect, &QAction::triggered, Driver, &DatabaseDriver::closeDatabase);
	connect(ui->actionAbout, &QAction::triggered, About, &AboutDialog::open);
}

MainWindow::~MainWindow(void)
{
	QSettings Settings("EW-Database");

	Settings.beginGroup("Builder");
	Settings.setValue("state", saveState());
	Settings.setValue("geometry", saveGeometry());
	Settings.endGroup();

	Thread.exit();
	Thread.wait();

	delete Driver;
	delete ui;
}

void MainWindow::lockUi(MainWindow::STATUS Status)
{
	switch (Status)
	{
		case CONNECTED:
			ui->centralWidget->setEnabled(true);
			ui->actionConnect->setEnabled(false);
			ui->actionDisconnect->setEnabled(true);
		break;
		case DISCONNECTED:
			ui->centralWidget->setEnabled(false);
			ui->actionConnect->setEnabled(true);
			ui->actionDisconnect->setEnabled(false);
		break;
		case BUSY:
			ui->Exec->setEnabled(false);
			ui->Exec->setVisible(false);
			ui->actionDisconnect->setEnabled(false);
		break;
		case DONE:
			ui->Exec->setEnabled(true);
			ui->Exec->setVisible(true);
			ui->actionDisconnect->setEnabled(true);
		break;
	}
}

void MainWindow::connectActionClicked(void)
{
	ConnectDialog* Dialog = new ConnectDialog(this); Dialog->open();

	connect(Dialog, &ConnectDialog::onAccept, this, &MainWindow::loginAttempt);

	connect(Dialog, &ConnectDialog::onAccept, Driver, &DatabaseDriver::openDatabase);
	connect(Dialog, &ConnectDialog::accepted, Dialog, &ConnectDialog::deleteLater);
	connect(Dialog, &ConnectDialog::rejected, Dialog, &ConnectDialog::deleteLater);

	connect(Driver, &DatabaseDriver::onLogin, Dialog, &ConnectDialog::connected);
	connect(Driver, &DatabaseDriver::onError, Dialog, &ConnectDialog::refused);
}

void MainWindow::proceedActionClicked(void)
{
	QHash<int, QVariant> Values; lockUi(BUSY);

	for (int i = 0; i < ui->valuesLayout->count(); ++i)
		if (auto W = qobject_cast<UpdateWidget*>(ui->valuesLayout->itemAt(i)->widget()))
			if (W->isChecked()) Values.insert(W->getIndex(), W->getValue());

	emit onExecRequest(Values, ui->Class->currentData().toString(),
				    ui->Line->currentData().toInt(),
				    ui->Point->currentData().toInt(),
				    ui->Text->currentData().toInt());
}

void MainWindow::databaseConnected(const QList<DatabaseDriver::TABLE>& Classes, unsigned Common, const QHash<QString, QHash<int, QString>>& Lines, const QHash<QString, QHash<int, QString>>& Points, const QHash<QString, QHash<int, QString>>& Texts)
{
	lineLayers = Lines; textLayers = Texts; pointLayers = Points; classesData = Classes; commonCount = Common;

	ui->Class->blockSignals(true); ui->Class->clear();

	for (const auto& Code : Classes) ui->Class->addItem(Code.Label, Code.Name);

	ui->Class->model()->sort(0); ui->Class->blockSignals(false);

	ui->Class->setCurrentIndex(0); lockUi(CONNECTED);
}

void MainWindow::databaseDisconnected(void)
{
	lockUi(DISCONNECTED);
}

void MainWindow::classIndexChanged(int Index)
{
	const QString Class = ui->Class->itemData(Index).toString();
	const QString Label = ui->Class->itemText(Index);

	const auto& Texts = textLayers[Class]; ui->Text->clear();
	const auto& Lines = lineLayers[Class]; ui->Line->clear();
	const auto& Points = pointLayers[Class]; ui->Point->clear();

	while (ui->valuesLayout->count())
	{
		ui->valuesLayout->takeAt(0)->widget()->deleteLater();
	}

	for (auto i = Texts.constBegin(); i != Texts.constEnd(); ++i)
	{
		ui->Text->addItem(i.value(), i.key());
	}

	for (auto i = Points.constBegin(); i != Points.constEnd(); ++i)
	{
		ui->Point->addItem(i.value(), i.key());
	}

	for (auto i = Lines.constBegin(); i != Lines.constEnd(); ++i)
	{
		ui->Line->addItem(i.value(), i.key());
	}

	const auto& Fields = getItemByField(classesData, Class, &DatabaseDriver::TABLE::Name).Fields;

	for (int i = 0; i < Fields.size(); ++i)
	{
		ui->valuesLayout->addWidget(new UpdateWidget(i, Fields[i], this));
	}

	ui->Text->model()->sort(0);
	ui->Point->model()->sort(0);
	ui->Line->model()->sort(0);

	ui->Text->setCurrentText(Label);
	ui->Point->setCurrentText(Label);
	ui->Line->setCurrentText(Label);

	ui->Text->setEnabled(ui->Text->count());
	ui->Point->setEnabled(ui->Point->count());
	ui->Line->setEnabled(ui->Line->count());
}

void MainWindow::databaseLogin(bool OK)
{
	ui->actionConnect->setEnabled(!OK);
}

void MainWindow::loginAttempt(void)
{
	ui->actionConnect->setEnabled(false);
}
