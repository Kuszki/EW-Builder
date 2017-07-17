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

#include "fitdialog.hpp"
#include "ui_fitdialog.h"

FitDialog::FitDialog(QWidget* Parent)
: QDialog(Parent), ui(new Ui::FitDialog)
{
	ui->setupUi(this); ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
}

FitDialog::~FitDialog(void)
{
	delete ui;
}

void FitDialog::accept(void)
{
	const QString Path = !ui->actionBox->currentIndex() ? ui->fileEdit->text() : QString();

	QDialog::accept(); emit onFitRequest(Path,
								  ui->xSpin->value(),
								  ui->ySpin->value(),
								  ui->radiusSpin->value());
}

void FitDialog::actionIndexChanged(int Index)
{
	const bool fromFile = !Index;

	ui->fileEdit->setEnabled(fromFile);
	ui->xSpin->setEnabled(fromFile);
	ui->ySpin->setEnabled(fromFile);
	ui->toolButton->setEnabled(fromFile);

	fitParametersChanged();
}

void FitDialog::fitParametersChanged(void)
{
	const bool fromFile = !ui->actionBox->currentIndex();

	QSet<int> Indexes; bool Accepted;

	Indexes.insert(ui->xSpin->value());
	Indexes.insert(ui->ySpin->value());

	Accepted = ui->radiusSpin->value() && (!fromFile || (
				  Indexes.size() == 2 &&
				  !ui->fileEdit->text().isEmpty()
			 ));

	ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(Accepted);
}

void FitDialog::openButtonClicked(void)
{
	const QString Path = QFileDialog::getOpenFileName(this, tr("Open data file"));

	if (!Path.isEmpty()) ui->fileEdit->setText(Path); fitParametersChanged();
}
