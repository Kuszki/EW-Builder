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

#include "updatewidget.hpp"
#include "ui_updatewidget.h"

UpdateWidget::UpdateWidget(int ID, const DatabaseDriver::FIELD& Field, QWidget* Parent)
: QWidget(Parent), ui(new Ui::UpdateWidget)
{
	ui->setupUi(this); setParameters(ID, Field); toggleWidget();

	connect(ui->Field, &QCheckBox::toggled, this, &UpdateWidget::onStatusChanged);
}

UpdateWidget::~UpdateWidget(void)
{
	delete ui;
}

QVariant UpdateWidget::getValue(void) const
{
	if (Simple && ui->exprButton->isChecked()) return Simple->text();

	if (auto W = dynamic_cast<QComboBox*>(Widget))
	{
		if (W->property("MASK").toBool())
		{
			auto M = dynamic_cast<QStandardItemModel*>(W->model());

			int Mask = 0;

			for (int i = 1; i < M->rowCount(); ++i)
				if (M->item(i)->checkState() == Qt::Checked)
				{
					Mask |= 1 << M->item(i)->data().toInt();
				}

			return Mask;
		}
		else
		{
			const auto Text = W->currentText();
			const int Index = W->findText(Text);

			if (Index == -1) return Text;
			else return W->itemData(Index);
		}
	}
	else if (auto W = dynamic_cast<QLineEdit*>(Widget))
	{
		return W->text().trimmed().replace("'", "''");
	}
	else if (auto W = dynamic_cast<QSpinBox*>(Widget))
	{
		return W->value();
	}
	else if (auto W = dynamic_cast<QDoubleSpinBox*>(Widget))
	{
		return W->value();
	}
	else if (auto W = dynamic_cast<QDateEdit*>(Widget))
	{
		return W->date().toString("dd.MM.yyyy");
	}
	else if (auto W = dynamic_cast<QDateTimeEdit*>(Widget))
	{
		return W->dateTime().toString("dd.MM.yyyy hh:mm:ss");
	}
	else return QVariant();
}

QString UpdateWidget::getLabel(void) const
{
	return ui->Field->text();
}

QString UpdateWidget::getName(void) const
{
	return ui->Field->toolTip();
}

int UpdateWidget::getIndex(void) const
{
	return Index;
}

void UpdateWidget::textChanged(const QString& Text)
{
	if (auto W = dynamic_cast<QComboBox*>(Widget))
	{
		emit onDataChecked(W->findText(Text) != -1);
	}
}

void UpdateWidget::toggleFunction(bool Expr)
{
	if (Widget) Widget->setVisible(!Expr || !Simple);
	if (Simple) Simple->setVisible(Expr);
}

void UpdateWidget::toggleWidget(void)
{
	if (Widget) Widget->setEnabled(ui->Field->isChecked());
	if (Simple) Simple->setEnabled(ui->Field->isChecked());
}

void UpdateWidget::undoClicked(void)
{
	setValue(Default);
}

void UpdateWidget::editFinished(void)
{
	emit onValueUpdate(objectName(), getValue());
}

void UpdateWidget::resetIndex(void)
{
	if (auto C = qobject_cast<QComboBox*>(sender())) C->setCurrentIndex(0);
}

void UpdateWidget::setParameters(int ID, const DatabaseDriver::FIELD& Field)
{
	ui->Field->setText(Field.Label); ui->Field->setToolTip(Field.Name); Index = ID;

	if (Widget) Widget->deleteLater(); Widget = nullptr;
	if (Simple) Simple->deleteLater(); Simple = nullptr;

	if (!Field.Dict.isEmpty()) switch (Field.Type)
	{
		case DatabaseDriver::MASK:
		{
			auto Combo = new QComboBox(this); Widget = Combo; int j = 1;
			auto Model = new QStandardItemModel(Field.Dict.size() + 1, 1, Widget);
			auto Item = new QStandardItem(tr("Select values"));

			for (auto i = Field.Dict.constBegin(); i != Field.Dict.constEnd(); ++i)
			{
				auto Item = new QStandardItem(i.value());

				Item->setData(i.key());
				Item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
				Item->setCheckState(Qt::Unchecked);

				Model->setItem(j++, Item);
			}

			Item->setFlags(Qt::ItemIsEnabled);
			Model->setItem(0, Item);
			Combo->setModel(Model);
			Combo->setProperty("MASK", true);

			connect(Combo, &QComboBox::currentTextChanged, this, &UpdateWidget::resetIndex);
		}
		break;
		default:
		{
			auto Combo = new QComboBox(this); Widget = Combo;

			for (auto i = Field.Dict.constBegin(); i != Field.Dict.constEnd(); ++i)
			{
				Combo->addItem(i.value(), i.key());
			}

			Combo->model()->sort(0);
			Combo->setProperty("MASK", false);

			connect(Combo, &QComboBox::editTextChanged, this, &UpdateWidget::textChanged);
		}
	}
	else switch (Field.Type)
	{
		case DatabaseDriver::INTEGER:
		case DatabaseDriver::SMALLINT:
		{
			auto Spin = new QSpinBox(this); Widget = Spin;

			Spin->setSingleStep(1);
			Spin->setRange(0, 10000);

			Simple = new QLineEdit(this);
		}
		break;
		case DatabaseDriver::BOOL:
		{
			auto Combo = new QComboBox(this); Widget = Combo;

			Combo->addItem(tr("Yes"), 1);
			Combo->addItem(tr("No"), 0);
			Combo->setProperty("MASK", false);
		}
		break;
		case DatabaseDriver::DOUBLE:
		{
			auto Spin = new QDoubleSpinBox(this); Widget = Spin;

			Spin->setSingleStep(1.0);
			Spin->setRange(0.0, 10000.0);

			Simple = new QLineEdit(this);
		}
		break;
		case DatabaseDriver::DATE:
		{
			auto Date = new QDateEdit(this); Widget = Date;

			Date->setDisplayFormat("dd.MM.yyyy");
			Date->setCalendarPopup(true);
		}
		break;
		case DatabaseDriver::DATETIME:
		{
			auto Date = new QDateTimeEdit(this); Widget = Date;

			Date->setDisplayFormat("dd.MM.yyyy hh:mm:ss");
			Date->setCalendarPopup(true);
		}
		break;
		default:
		{
			auto Edit = new QLineEdit(this); Widget = Edit;

			Edit->setClearButtonEnabled(true);
		}
		break;
	}

	if (Widget)
	{
		Widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		Widget->setEnabled(ui->Field->isChecked());

		ui->horizontalLayout->insertWidget(1, Widget);
	}

	if (Simple)
	{
		Simple->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		Simple->setEnabled(ui->Field->isChecked());
		Simple->setClearButtonEnabled(true);

		ui->horizontalLayout->insertWidget(1, Simple);

		ui->exprButton->setVisible(true);
		ui->exprButton->setEnabled(ui->Field->isChecked());
	}
	else
	{
		ui->exprButton->setVisible(false);
		ui->exprButton->setEnabled(false);
	}

	setObjectName(Field.Name); toggleFunction(ui->exprButton->isChecked());
}

void UpdateWidget::setChecked(bool Checked)
{
	ui->Field->setChecked(Checked);
}

void UpdateWidget::setValue(const QVariant& Value)
{
	if (auto W = dynamic_cast<QComboBox*>(Widget))
	{
		if (W->property("MASK").toBool())
		{
			auto M = dynamic_cast<QStandardItemModel*>(W->model());

			for (int i = 1; i < M->rowCount(); ++i)
			{
				const bool Checked = Value.toInt() & (1 << M->item(i)->data().toInt());

				M->item(i)->setCheckState(Checked ? Qt::Checked : Qt::Unchecked);
			}

		}
		else
		{
			W->setCurrentIndex(W->findData(Value));
		}

	}
	else if (auto W = dynamic_cast<QLineEdit*>(Widget))
	{
		W->setText(Value.toString());
	}
	else if (auto W = dynamic_cast<QSpinBox*>(Widget))
	{
		W->setValue(Value.toInt());
	}
	else if (auto W = dynamic_cast<QDoubleSpinBox*>(Widget))
	{
		W->setValue(Value.toDouble());
	}
	else if (auto W = dynamic_cast<QDateEdit*>(Widget))
	{
		W->setDate(Value.toDate());
	}
	else if (auto W = dynamic_cast<QDateTimeEdit*>(Widget))
	{
		W->setDateTime(Value.toDateTime());
	}

	if (Value.isValid()) Default = Value;
}

bool UpdateWidget::isChecked(void) const
{
	return isEnabled() && ui->Field->isChecked();
}

void UpdateWidget::reset(void)
{
	setChecked(false); setValue(QVariant());
}
