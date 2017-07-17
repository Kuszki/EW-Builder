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
	About = new AboutDialog(this);
	Proceed = new ProceedDialog(this);
	Jobs = new JobsDialog(this);
	Geometry = new FitDialog(this);
	Progress = new QProgressBar(this);

	Progress->hide();
	Driver->moveToThread(&Thread);
	Thread.start();

	ui->valuesLayout->setAlignment(Qt::AlignTop);
	ui->statusBar->addPermanentWidget(Progress);

	QSettings Settings("EW-Database");

	Settings.beginGroup("Builder");
	restoreGeometry(Settings.value("geometry").toByteArray());
	restoreState(Settings.value("state").toByteArray());
	Settings.endGroup();

	connect(Driver, &DatabaseDriver::onBeginProgress, Progress, &QProgressBar::show);
	connect(Driver, &DatabaseDriver::onSetupProgress, Progress, &QProgressBar::setRange);
	connect(Driver, &DatabaseDriver::onUpdateProgress, Progress, &QProgressBar::setValue);
	connect(Driver, &DatabaseDriver::onEndProgress, Progress, &QProgressBar::hide);

	connect(Driver, &DatabaseDriver::onConnect, this, &MainWindow::databaseConnected);
	connect(Driver, &DatabaseDriver::onDisconnect, this, &MainWindow::databaseDisconnected);
	connect(Driver, &DatabaseDriver::onLogin, this, &MainWindow::databaseLogin);

	connect(this, &MainWindow::onExecRequest, Driver, &DatabaseDriver::proceedClass);
	connect(this, &MainWindow::onJobsRequest, Driver, &DatabaseDriver::proceedJobs);
	connect(this, &MainWindow::onFitRequest, Driver, &DatabaseDriver::proceedFit);
	connect(Driver, &DatabaseDriver::onProceedEnd, this, &MainWindow::execProcessEnd);

	connect(ui->actionConnect, &QAction::triggered, this, &MainWindow::connectActionClicked);
	connect(ui->actionDisconnect, &QAction::triggered, Driver, &DatabaseDriver::closeDatabase);
	connect(ui->actionProceed, &QAction::triggered, Proceed, &ProceedDialog::open);
	connect(ui->actionJobs, &QAction::triggered, Jobs, &JobsDialog::open);
	connect(ui->actionGeometry, &QAction::triggered, Geometry, &FitDialog::open);
	connect(ui->actionCancel, &QAction::triggered, this, &MainWindow::cancelActionClicked);
	connect(ui->actionAbout, &QAction::triggered, About, &AboutDialog::open);

	connect(Proceed, &ProceedDialog::onProceedRequest, this, &MainWindow::proceedRequest);
	connect(Jobs, &JobsDialog::onFitRequest, this, &MainWindow::jobsRequest);
	connect(Geometry, &FitDialog::onFitRequest, this, &MainWindow::fitRequest);

	connect(Driver, SIGNAL(onBeginProgress(QString)), ui->statusBar, SLOT(showMessage(QString)));
	connect(Driver, SIGNAL(onError(QString)), ui->statusBar, SLOT(showMessage(QString)));
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
			ui->actionProceed->setEnabled(true);
			ui->actionJobs->setEnabled(true);
			ui->actionGeometry->setEnabled(true);
			ui->actionCancel->setEnabled(false);
		break;
		case DISCONNECTED:
			ui->centralWidget->setEnabled(false);
			ui->actionConnect->setEnabled(true);
			ui->actionDisconnect->setEnabled(false);
			ui->actionProceed->setEnabled(false);
			ui->actionJobs->setEnabled(false);
			ui->actionGeometry->setEnabled(false);
			ui->actionCancel->setEnabled(false);
		break;
		case BUSY:
			ui->actionProceed->setEnabled(false);
			ui->actionJobs->setEnabled(false);
			ui->actionGeometry->setEnabled(false);
			ui->actionDisconnect->setEnabled(false);
			ui->actionCancel->setEnabled(true);
		break;
		case DONE:
			ui->actionProceed->setEnabled(true);
			ui->actionJobs->setEnabled(true);
			ui->actionGeometry->setEnabled(true);
			ui->actionDisconnect->setEnabled(true);
			ui->actionCancel->setEnabled(false);
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

void MainWindow::cancelActionClicked(void)
{
	Driver->terminate();
}

void MainWindow::proceedRequest(double Length, bool Line, bool Job, int Point, const QString& Symbol)
{
	QHash<int, QVariant> Values; lockUi(BUSY);

	for (int i = 0; i < ui->valuesLayout->count(); ++i)
		if (auto W = qobject_cast<UpdateWidget*>(ui->valuesLayout->itemAt(i)->widget()))
			if (W->isChecked()) Values.insert(W->getIndex(), W->getValue());

	emit onExecRequest(Values, ui->Pattern->text(),
				    ui->Class->currentData().toString(),
				    ui->Line->currentData().toInt(),
				    ui->Point->currentData().toInt(),
				    ui->Text->currentData().toInt(),
				    Length, Line, Job, Point, Symbol);
}

void MainWindow::jobsRequest(const QString& Path, const QString& Sep, int xPos, int yPos, int jobPos)
{
	lockUi(BUSY); emit onJobsRequest(Path, Sep, xPos, yPos, jobPos);
}

void MainWindow::fitRequest(const QString& Path, int xPos, int yPos, double Radius)
{
	lockUi(BUSY); emit onFitRequest(Path, xPos, yPos, Radius);
}

void MainWindow::databaseConnected(const QList<DatabaseDriver::TABLE>& Classes, unsigned Common, const QHash<QString, QHash<int, QString>>& Lines, const QHash<QString, QHash<int, QString>>& Points, const QHash<QString, QHash<int, QString>>& Texts)
{
	lineLayers = Lines; textLayers = Texts; pointLayers = Points; classesData = Classes; commonCount = Common;

	ui->Class->blockSignals(true);

	ui->Class->clear();

	for (const auto& Code : Classes) ui->Class->addItem(Code.Label, Code.Name);

	ui->Class->model()->sort(0);

	ui->Class->blockSignals(false);

	ui->Class->setCurrentIndex(0);

	lockUi(CONNECTED); ui->statusBar->showMessage(tr("Database connected"));
}

void MainWindow::databaseDisconnected(void)
{
	lockUi(DISCONNECTED); ui->statusBar->showMessage(tr("Database disconnected"));
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

void MainWindow::execProcessEnd(int Count)
{
	lockUi(DONE); ui->statusBar->showMessage(tr("Processed %n item(s)", nullptr, Count));
}

void MainWindow::databaseLogin(bool OK)
{
	ui->actionConnect->setEnabled(!OK);
}

void MainWindow::loginAttempt(void)
{
	ui->actionConnect->setEnabled(false);
}
