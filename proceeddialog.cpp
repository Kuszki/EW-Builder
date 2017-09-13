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

#include "proceeddialog.hpp"
#include "ui_proceeddialog.h"

ProceedDialog::ProceedDialog(QWidget* Parent)
: QDialog(Parent), ui(new Ui::ProceedDialog)
{
	ui->setupUi(this); static const QStringList Policy = { tr("Check label job"), tr("Check segment job") };

	auto Model = new QStandardItemModel(0, 1, this);
	auto Item = new QStandardItem(tr("Job policy"));

	int i(0); for (const auto& Text : Policy)
	{
		auto Item = new QStandardItem(Text);

		Item->setData(i);
		Item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
		Item->setCheckState(Qt::Unchecked);

		Model->insertRow(i++, Item);
	}

	Item->setFlags(Qt::ItemIsEnabled);
	Model->insertRow(0, Item);

	ui->policyCombo->setModel(Model);
}

ProceedDialog::~ProceedDialog(void)
{
	delete ui;
}

void ProceedDialog::symbolTextChanged(const QString& Text)
{
	const bool Ok = !Text.isEmpty() || !ui->symbolEdit->isEnabled();

	ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(Ok);
}

void ProceedDialog::pointStrategyChanged(int Index)
{
	ui->symbolEdit->setEnabled(Index); symbolTextChanged(ui->symbolEdit->text());
}

void ProceedDialog::accept(void)
{
	const QString Symbol = ui->pointCombo->currentIndex() ? ui->symbolEdit->text() : QString();
	const double Length = ui->distanceSpin->value() == 0.0 ? qInf() : ui->distanceSpin->value();
	const double Spin = (ui->rotationSpin->value() / 180.0) * M_PI;

	int Mask(0); auto M = dynamic_cast<QStandardItemModel*>(ui->policyCombo->model());

	for (int i = 1; i < M->rowCount(); ++i)
		if (M->item(i)->checkState() == Qt::Checked)
		{
			Mask |= (1 << (i - 1));
		}

	QDialog::accept(); emit onProceedRequest(Length, Spin,
									 ui->lineCombo->currentIndex(),
									 Mask,
									 ui->pointCombo->currentIndex(),
									 Symbol);
}
