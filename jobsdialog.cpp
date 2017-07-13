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

#include "jobsdialog.hpp"
#include "ui_jobsdialog.h"

JobsDialog::JobsDialog(QWidget *Parent)
: QDialog(Parent), ui(new Ui::JobsDialog)
{
	ui->setupUi(this); ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
}

JobsDialog::~JobsDialog(void)
{
	delete ui;
}

void JobsDialog::accept(void)
{
	const QString Sep = ui->truncateSpin->isChecked() ? ui->separatorEdit->text() : QString();

	QDialog::accept(); emit onFitRequest(ui->fileEdit->text(), Sep,
								  ui->xSpin->value(),
								  ui->ySpin->value(),
								  ui->jobSpin->value());
}

void JobsDialog::openButtonClicked(void)
{
	const QString Path = QFileDialog::getOpenFileName(this, tr("Open data file"));

	if (!Path.isEmpty()) ui->fileEdit->setText(Path); dialogDataChanged();
}

void JobsDialog::dialogDataChanged(void)
{
	QSet<int> Indexes; bool Accepted;

	Indexes.insert(ui->xSpin->value());
	Indexes.insert(ui->ySpin->value());
	Indexes.insert(ui->jobSpin->value());

	Accepted = (!ui->separatorEdit->isEnabled() ||
			  !ui->separatorEdit->text().isEmpty()) &&
			 !ui->fileEdit->text().isEmpty() &&
			 Indexes.size() == 3;

	ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(Accepted);
}
