/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  EW-Mapa object creator                                                 *
 *  Copyright (C) 2017  Łukasz "Kuszki" Dróżdż  l.drozdz@openmailbox.org   *
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

#include "labeldialog.hpp"
#include "ui_labeldialog.h"

LabelDialog::LabelDialog(QMap<QString, QString> Classes, QHash<QString, QHash<int, QString>> Text, QWidget *Parent)
: QDialog(Parent), ui(new Ui::LabelDialog)
{
	ui->setupUi(this); setParameters(Classes, Text);
}

LabelDialog::~LabelDialog(void)
{
	delete ui;
}

void LabelDialog::currentClassChanged(int Index)
{
	const QString Class = ui->classCombo->itemData(Index).toString();

	const auto& List = textLayers[Class]; ui->destonationCombo->clear();

	for (auto i = List.constBegin(); i != List.constEnd(); ++i)
	{
		ui->destonationCombo->addItem(i.value(), i.key());
	}

	ui->destonationCombo->model()->sort(0);
	ui->destonationCombo->setCurrentIndex(0);
}

void LabelDialog::accept(void)
{
	QDialog::accept();

	emit onLabelsRequest(ui->classCombo->currentData().toString(),
					 ui->sourceCombo->currentData().toInt(),
					 ui->destonationCombo->currentData().toInt(),
					 ui->distanceSpin->value(),
					 ui->rotationSpin->value(),
					 ui->infoCheck->isChecked());
}

void LabelDialog::setParameters(QMap<QString, QString> Classes, QHash<QString, QHash<int, QString>> Text)
{
	textLayers = Text; ui->classCombo->clear(); QHash<int, QString> Texts;

	for (auto i = Classes.constBegin(); i != Classes.constEnd(); ++i)
		for (auto j = Text[i.key()].constBegin(); j != Text[i.key()].constEnd(); ++j)
		{
			Texts.insert(j.key(), QString("%1: %2").arg(i.value()).arg(j.value()));
		}

	for (auto i = Classes.constBegin(); i != Classes.constEnd(); ++i)
	{
		ui->classCombo->addItem(i.value(), i.key());
	}

	for (auto i = Texts.constBegin(); i != Texts.constEnd(); ++i)
	{
		ui->sourceCombo->addItem(i.value(), i.key());
	}

	ui->classCombo->model()->sort(0); ui->classCombo->setCurrentIndex(0);
	ui->sourceCombo->model()->sort(0); ui->sourceCombo->setCurrentIndex(0);
}
