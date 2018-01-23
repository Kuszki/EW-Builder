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

#include "duplicatesdialog.hpp"
#include "ui_duplicatesdialog.h"

DuplicatesDialog::DuplicatesDialog(const QList<DatabaseDriver::LAYER> Lines, const QList<DatabaseDriver::LAYER> Texts, QWidget* Parent)
: QDialog(Parent), ui(new Ui::DuplicatesDialog)
{
	ui->setupUi(this); setParameters(Lines, Texts);
}

DuplicatesDialog::~DuplicatesDialog(void)
{
	delete ui;
}

void DuplicatesDialog::actionIndexChanged(int Index)
{
	ui->heursticCombo->setEnabled(Index == 0);
	ui->heursticLabel->setEnabled(Index == 0);
}

void DuplicatesDialog::strategyIndexChanged(int Index)
{
	ui->sublayerCombo->setEnabled(Index < 1);
	ui->sublayerLabel->setEnabled(Index < 1);

	ui->layerCombo->setEnabled(Index < 2);
	ui->layerLabel->setEnabled(Index < 2);
}

void DuplicatesDialog::typeIndexChanged(int Index)
{
	const auto& List = Index == 0 ? textLayers : lineLayers;

	ui->layerCombo->blockSignals(true);
	ui->layerCombo->clear();

	for (const auto& Layer : List) ui->layerCombo->addItem(Layer.Label, Layer.ID);

	ui->layerCombo->model()->sort(0);
	ui->layerCombo->setCurrentIndex(0);

	ui->layerCombo->blockSignals(false);

	layerIndexChanged(ui->layerCombo->currentIndex());
}

void DuplicatesDialog::layerIndexChanged(int Index)
{
	const auto& List = ui->typeCombo->currentIndex() == 0 ? textLayers : lineLayers;
	const int ID = ui->layerCombo->currentData().toInt();

	int i(0), j(0); for (const auto& Layer : List)
	{
		if (Layer.ID == ID) i = j; ++j;
	}

	ui->sublayerCombo->clear();

	for (const auto& Sublayer : List[i].Sublayers) ui->sublayerCombo->addItem(Sublayer.Label, Sublayer.ID);

	ui->sublayerCombo->model()->sort(0);
	ui->sublayerCombo->setCurrentIndex(0);
}

void DuplicatesDialog::accept(void)
{
	QDialog::accept();

	emit onRemoveRequest(
				ui->actionCombo->currentIndex(),
				ui->strategyCombo->currentIndex(),
				ui->heursticCombo->currentIndex(),
				ui->typeCombo->currentIndex(),
				ui->layerCombo->currentData().toInt(),
				ui->sublayerCombo->currentData().toInt(),
				ui->distanceSpin->value());
}

void DuplicatesDialog::setParameters(const QList<DatabaseDriver::LAYER> Lines, const QList<DatabaseDriver::LAYER> Texts)
{
	lineLayers = Lines; textLayers = Texts;

	typeIndexChanged(ui->typeCombo->currentIndex());
}
