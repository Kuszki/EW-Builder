/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  EW-Mapa object creator                                                 *
 *  Copyright (C) 2016  Łukasz "Kuszki" Dróżdż  l.drozdz@o2.pl             *
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

#include "hidedialog.hpp"
#include "ui_hidedialog.h"

HideDialog::HideDialog(const QHash<int, QString>& Hides, QWidget* Parent)
: QDialog(Parent), ui(new Ui::HideDialog)
{
	ui->setupUi(this); setFields(Hides);

	ui->fieldsLayout->setAlignment(Qt::AlignTop);
}

HideDialog::~HideDialog(void)
{
	delete ui;
}

QSet<int> HideDialog::getSelectedFields(void) const
{
	QSet<int> List;

	for (int i = 0; i < ui->fieldsLayout->count(); ++i)
		if (auto W = qobject_cast<QCheckBox*>(ui->fieldsLayout->itemAt(i)->widget()))
			if (W->isEnabled() && W->isChecked()) List.insert(W->property("ID").toInt());

	return List;
}

void HideDialog::accept(void)
{
	QDialog::accept(); emit onHideRequest(getSelectedFields(), ui->objectedCheck->isChecked());
}

void HideDialog::setFields(const QHash<int, QString>& List)
{
	while (auto I = ui->fieldsLayout->takeAt(0)) if (auto W = I->widget()) W->deleteLater();

	for (auto i = List.constBegin(); i != List.constEnd(); ++i)
	{
		auto Widget = new QCheckBox(this);

		Widget->setText(i.value());
		Widget->setProperty("ID", i.key());

		ui->fieldsLayout->addWidget(Widget);
	}
}

void HideDialog::setUnchecked(void)
{
	for (int i = 0; i < ui->fieldsLayout->count(); ++i)
		if (auto W = dynamic_cast<QCheckBox*>(ui->fieldsLayout->itemAt(i)->widget()))
		{
			W->setChecked(false);
		}
}
