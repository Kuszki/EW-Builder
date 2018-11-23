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

#include "databasedriver.hpp"

DatabaseDriver::DatabaseDriver(QObject* Parent)
: QObject(Parent)
{
	QSettings Settings("EW-Database");

	Settings.beginGroup("Database");
	Database = QSqlDatabase::addDatabase(Settings.value("driver", "QIBASE").toString());
	Settings.endGroup();
}

DatabaseDriver::~DatabaseDriver(void) {}

bool DatabaseDriver::isConnected(void) const
{
	QMutexLocker Locker(&Terminator); return Database.isOpen();
}

QList<DatabaseDriver::FIELD> DatabaseDriver::loadCommon(bool Emit)
{
	if (!Database.isOpen()) return QList<FIELD>();

	QList<FIELD> Fields =
	{
		{ INTEGER,	"EW_OBIEKTY.OPERAT",	tr("Job name")			},
		{ READONLY,	"EW_OBIEKTY.KOD",		tr("Object code")		},
		{ READONLY,	"EW_OBIEKTY.NUMER",		tr("Object ID")		},
		{ DATETIME,	"EW_OBIEKTY.DTU",		tr("Creation date")		},
		{ DATETIME,	"EW_OBIEKTY.DTW",		tr("Modification date")	},
		{ INTEGER,	"EW_OBIEKTY.OSOU",		tr("Created by")		},
		{ INTEGER,	"EW_OBIEKTY.OSOW",		tr("Modified by")		}
	};

	QHash<QString, QString> Dict =
	{
		{ "EW_OBIEKTY.OPERAT",		"SELECT UID, NUMER FROM EW_OPERATY"	},
		{ "EW_OBIEKTY.KOD",			"SELECT KOD, OPIS FROM EW_OB_OPISY"	},
		{ "EW_OBIEKTY.OSOU",		"SELECT ID, NAME FROM EW_USERS"		},
		{ "EW_OBIEKTY.OSOW",		"SELECT ID, NAME FROM EW_USERS"		}
	};

	if (Emit) emit onSetupProgress(0, Dict.size());

	int j = 0; for (auto i = Dict.constBegin(); i != Dict.constEnd(); ++i)
	{
		auto& Field = getItemByField(Fields, i.key(), &FIELD::Name);

		if (Field.Name.isEmpty()) continue;

		QSqlQuery Query(i.value(), Database); Query.setForwardOnly(true);

		if (Query.exec()) while (Query.next()) Field.Dict.insert(Query.value(0), Query.value(1).toString());

		if (Emit) emit onUpdateProgress(++j);
	}

	return Fields;
}

QList<DatabaseDriver::TABLE> DatabaseDriver::loadTables(bool Emit)
{
	if (!Database.isOpen()) return QList<TABLE>();

	QSqlQuery Query(Database); Query.setForwardOnly(true);
	QList<TABLE> List; int Step = 0;

	if (Emit && Query.exec("SELECT COUNT(*) FROM EW_OB_OPISY") && Query.next())
	{
		emit onSetupProgress(0, Query.value(0).toInt());
	}

	Query.prepare(
		"SELECT "
			"O.KOD, O.OPIS, O.DANE_DOD, O.OPCJE, "
			"F.NAZWA, F.TYTUL, F.TYP, "
			"D.WARTOSC, D.OPIS "
		"FROM "
			"EW_OB_OPISY O "
		"INNER JOIN "
			"EW_OB_DDSTR F "
		"ON "
			"O.KOD = F.KOD "
		"INNER JOIN "
			"EW_OB_DDSTR S "
		"ON "
			"F.KOD = S.KOD "
		"LEFT JOIN "
			"EW_OB_DDSL D "
		"ON "
			"S.UID = D.UIDP OR S.UIDSL = D.UIDP "
		"WHERE "
			"S.NAZWA = F.NAZWA "
		"ORDER BY "
			"O.KOD, F.NAZWA, D.OPIS");

	if (Query.exec()) while (Query.next())
	{
		const QString Field = QString("EW_DATA.%1").arg(Query.value(4).toString());
		const QString Table = Query.value(0).toString();
		const bool Dict = !Query.value(7).isNull();

		if (!hasItemByField(List, Table, &TABLE::Name)) List.append(
		{
			Table,
			Query.value(1).toString(),
			Query.value(2).toString(),
			Query.value(3).toInt()
		});

		auto& Tabref = getItemByField(List, Table, &TABLE::Name);

		if (!hasItemByField(Tabref.Fields, Field, &FIELD::Name)) Tabref.Fields.append(
		{
			TYPE(Query.value(6).toInt()),
			Field,
			Query.value(5).toString()
		});

		auto& Fieldref = getItemByField(Tabref.Fields, Field, &FIELD::Name);

		if (Dict) Fieldref.Dict.insert(Query.value(7), Query.value(8).toString());

		if (Emit && Step != List.size()) emit onUpdateProgress(Step = List.size());
	}

	QtConcurrent::blockingMap(List, [] (TABLE& Table) -> void
	{
		for (auto& Field : Table.Fields) if (Field.Type == INTEGER && !Field.Dict.isEmpty())
		{
			if (!Field.Dict.contains(0)) Field.Dict.insert(0, tr("Unknown"));
		}
	});

	if (Emit) emit onEndProgress(); return List;
}

QList<DatabaseDriver::LAYER> DatabaseDriver::loadLayers(unsigned Type)
{
	if (!Database.isOpen()) return QList<LAYER>(); QList<LAYER> Layers;

	QSqlQuery Query(Database); Query.setForwardOnly(true);

	if (Type == 0) Query.prepare(
		"SELECT "
			"G.ID, L.ID, G.NAZWA, G.NAZWA_L, L.NAZWA, L.DLUGA_NAZWA "
		"FROM "
			"EW_WARSTWA_LINIOWA L "
		"INNER JOIN "
			"EW_GRUPY_WARSTW G "
		"ON "
			"L.ID_GRUPY = G.ID");
	else if (Type == 1) Query.prepare(
		"SELECT "
			"G.ID, L.ID, G.NAZWA, G.NAZWA_L, L.NAZWA, L.DLUGA_NAZWA "
		"FROM "
			"EW_WARSTWA_TEXTOWA L "
		"INNER JOIN "
			"EW_GRUPY_WARSTW G "
		"ON "
			"L.ID_GRUPY = G.ID");

	if (Query.exec()) while (Query.next())
	{
		const int GID = Query.value(0).toInt();
		const int LID = Query.value(1).toInt();

		if (!hasItemByField(Layers, GID, &LAYER::ID)) Layers.append(
		{
			GID, Query.value(2).toString(), Query.value(3).toString()
		});

		LAYER& Layer = getItemByField(Layers, GID, &LAYER::ID);

		if (!hasItemByField(Layer.Sublayers, LID, &SUBLAYER::ID)) Layer.Sublayers.append(
		{
			LID, Query.value(4).toString(), Query.value(5).toString()
		});
	}

	return Layers;
}

QList<DatabaseDriver::FIELD> DatabaseDriver::loadFields(const QString& Table) const
{
	if (!Database.isOpen()) return QList<FIELD>();

	QSqlQuery Query(Database); Query.setForwardOnly(true);
	QList<FIELD> List;

	Query.prepare(
		"SELECT "
			"D.NAZWA, D.TYTUL, D.TYP "
		"FROM "
			"EW_OB_DDSTR D "
		"WHERE "
			"D.KOD = :table");

	Query.bindValue(":table", Table);

	if (Query.exec()) while (Query.next())
	{
		const QString Data = Query.value(0).toString();

		List.append(
		{
			TYPE(Query.value(2).toInt()),
			QString("EW_DATA.%1").arg(Query.value(0).toString()),
			Query.value(1).toString(),
			loadDict(Data, Table)
		});

		if (List.last().Type == INTEGER && !List.last().Dict.isEmpty())
		{
			if (!List.last().Dict.contains(0)) List.last().Dict.insert(0, tr("Unknown"));
		}
	}

	return List;
}

QMap<QVariant, QString> DatabaseDriver::loadDict(const QString& Field, const QString& Table) const
{
	if (!Database.isOpen()) return QMap<QVariant, QString>();

	QSqlQuery Query(Database); Query.setForwardOnly(true);
	QMap<QVariant, QString> List;

	Query.prepare(
		"SELECT "
			"L.WARTOSC, L.OPIS "
		"FROM "
			"EW_OB_DDSL L "
		"INNER JOIN "
			"EW_OB_DDSTR R "
		"ON "
			"L.UIDP = R.UID "
		"WHERE "
			"R.NAZWA = :field AND R.KOD = :table "
		"ORDER BY "
			"L.OPIS");

	Query.bindValue(":field", Field);
	Query.bindValue(":table", Table);

	if (Query.exec()) while (Query.next()) List.insert(Query.value(0), Query.value(1).toString());

	Query.prepare(
		"SELECT "
			"L.WARTOSC, L.OPIS "
		"FROM "
			"EW_OB_DDSL L "
		"INNER JOIN "
			"EW_OB_DDSTR R "
		"ON "
			"L.UIDP = R.UIDSL "
		"WHERE "
			"R.NAZWA = :field AND R.KOD = :table "
		"ORDER BY "
			"L.OPIS");

	Query.bindValue(":field", Field);
	Query.bindValue(":table", Table);

	if (Query.exec()) while (Query.next()) List.insert(Query.value(0), Query.value(1).toString());

	return List;
}

QList<DatabaseDriver::FIELD> DatabaseDriver::normalizeFields(QList<DatabaseDriver::TABLE>& Tabs, const QList<DatabaseDriver::FIELD>& Base) const
{
	QList<FIELD> List;

	for (const auto& Field : Base)
	{
		if (!List.contains(Field)) List.append(Field);
	}

	for (const auto& Tab : Tabs) for (const auto& Field : Tab.Fields)
	{
		if (!List.contains(Field)) List.append(Field);
	}

	QtConcurrent::blockingMap(Tabs, [&List] (TABLE& Tab) -> void
	{
		for (auto& Field : Tab.Fields) Tab.Indexes.append(List.indexOf(Field));
	});

	return List;
}

QStringList DatabaseDriver::normalizeHeaders(QList<DatabaseDriver::TABLE>& Tabs, const QList<DatabaseDriver::FIELD>& Base) const
{
	QStringList List;

	for (const auto& Field : Base)
	{
		if (!List.contains(Field.Label)) List.append(Field.Label);
	}

	for (const auto& Tab : Tabs) for (const auto& Field : Tab.Fields)
	{
		if (!List.contains(Field.Label)) List.append(Field.Label);
	}

	QtConcurrent::blockingMap(Tabs, [&List] (TABLE& Tab) -> void
	{
		for (auto& Field : Tab.Fields) Tab.Headers.append(List.indexOf(Field.Label));
	});

	return List;
}

QHash<QString, QHash<int, QString>> DatabaseDriver::loadLineLayers(const QList<DatabaseDriver::TABLE>& Tabs, bool Hide)
{
	if (!Database.isOpen()) return QHash<QString, QHash<int, QString>>();

	QSqlQuery Query(Database); Query.setForwardOnly(true);
	QHash<QString, QHash<int, QString>> lineLayers; QSet<int> Used;

	if (Hide)
	{
		Query.prepare(
			"SELECT DISTINCT "
				"P.ID_WARSTWY "
			"FROM "
				"EW_POLYLINE P "
			"WHERE "
				"P.STAN_ZMIANY = 0 AND "
				"P.ID NOT IN ("
					"SELECT "
						"E.IDE "
					"FROM "
						"EW_OB_ELEMENTY E "
					"INNER JOIN "
						"EW_OBIEKTY O "
					"ON "
						"O.UID = E.UIDO "
					"WHERE "
						"O.STATUS = 0 AND "
						"E.TYP = 0"
				")");

		if (Query.exec()) while (Query.next()) Used.insert(Query.value(0).toInt());
	}

	for (const auto& Table : Tabs)
	{
		QHash<int, QString> L, OUT;

		{
			Query.prepare(QString(
				"SELECT "
					"L.ID, G.NAZWA_L "
				"FROM "
					"EW_WARSTWA_LINIOWA L "
				"INNER JOIN "
					"EW_GRUPY_WARSTW G "
				"ON "
					"L.ID_GRUPY = G.ID "
				"INNER JOIN "
					"EW_OB_KODY_OPISY O "
				"ON "
					"G.ID = O.ID_WARSTWY "
				"WHERE "
					"O.KOD = '%1' AND "
					"L.NAZWA LIKE (O.KOD || '%') "
				"ORDER BY "
					"G.NAZWA_L")
					    .arg(Table.Name));

			if (Query.exec()) while (Query.next())
			{
				L.insert(Query.value(0).toInt(), Query.value(1).toString());
			}
		}

		if (L.isEmpty())
		{
			Query.prepare(QString(
				"SELECT "
					"L.ID, L.DLUGA_NAZWA "
				"FROM "
					"EW_WARSTWA_LINIOWA L "
				"INNER JOIN "
					"EW_GRUPY_WARSTW G "
				"ON "
					"L.ID_GRUPY = G.ID "
				"INNER JOIN "
					"EW_OB_KODY_OPISY O "
				"ON "
					"G.ID = O.ID_WARSTWY "
				"WHERE "
					"O.KOD = '%1' AND "
					"L.NAZWA LIKE (SUBSTRING(O.KOD FROM 1 FOR 4) || '%') "
				"ORDER BY "
					"L.DLUGA_NAZWA")
					    .arg(Table.Name));

			if (Query.exec()) while (Query.next())
			{
				L.insert(Query.value(0).toInt(), Query.value(1).toString());
			}
		}

		if (!Hide) lineLayers.insert(Table.Name, L);
		else
		{
			for (const auto& ID : L.keys()) if (Used.contains(ID))
			{
				OUT.insert(ID, L.value(ID));
			}

			lineLayers.insert(Table.Name, OUT);
		}
	}

	return lineLayers;
}

QHash<QString, QHash<int, QString>> DatabaseDriver::loadPointLayers(const QList<TABLE>& Tabs, bool Hide)
{
	if (!Database.isOpen()) return QHash<QString, QHash<int, QString>>();

	QSqlQuery Query(Database); Query.setForwardOnly(true);
	QHash<QString, QHash<int, QString>> pointLayers; QSet<int> Used;

	if (Hide)
	{
		Query.prepare(
			"SELECT DISTINCT "
				"P.ID_WARSTWY "
			"FROM "
				"EW_TEXT P "
			"WHERE "
				"P.STAN_ZMIANY = 0 AND "
				"P.TYP = 4 AND "
				"P.ID NOT IN ("
					"SELECT "
						"E.IDE "
					"FROM "
						"EW_OB_ELEMENTY E "
					"INNER JOIN "
						"EW_OBIEKTY O "
					"ON "
						"O.UID = E.UIDO "
					"WHERE "
						"O.STATUS = 0 AND "
						"E.TYP = 0"
				")");

		if (Query.exec()) while (Query.next()) Used.insert(Query.value(0).toInt());
	}

	for (const auto& Table : Tabs)
	{
		QHash<int, QString> P, OUT;

		{
			Query.prepare(QString(
				"SELECT "
					"T.ID, G.NAZWA_L "
				"FROM "
					"EW_WARSTWA_TEXTOWA T "
				"INNER JOIN "
					"EW_GRUPY_WARSTW G "
				"ON "
					"T.ID_GRUPY = G.ID "
				"INNER JOIN "
					"EW_OB_KODY_OPISY O "
				"ON "
					"G.ID = O.ID_WARSTWY "
				"WHERE "
					"O.KOD = '%1' AND "
					"T.NAZWA = O.KOD AND "
					"G.NAZWA NOT LIKE '%#_E' "
				"ESCAPE "
					"'#' "
				"ORDER BY "
					"G.NAZWA_L")
					    .arg(Table.Name));

			if (Query.exec()) while (Query.next())
			{
				P.insert(Query.value(0).toInt(), Query.value(1).toString());
			}
		}

		{
			Query.prepare(QString(
				"SELECT "
					"T.ID, G.NAZWA_L "
				"FROM "
					"EW_WARSTWA_TEXTOWA T "
				"INNER JOIN "
					"EW_GRUPY_WARSTW G "
				"ON "
					"T.ID_GRUPY = G.ID "
				"INNER JOIN "
					"EW_OB_KODY_OPISY O "
				"ON "
					"G.ID = O.ID_WARSTWY "
				"WHERE "
					"O.KOD = '%1' AND "
					"T.NAZWA LIKE (O.KOD || '#_%') "
				"ESCAPE "
					"'#' "
				"ORDER BY "
					"G.NAZWA_L")
					    .arg(Table.Name));

			if (Query.exec()) while (Query.next())
			{
				P.insert(Query.value(0).toInt(), Query.value(1).toString());
			}
		}

		if (!Hide) pointLayers.insert(Table.Name, P);
		else
		{
			for (const auto& ID : P.keys()) if (Used.contains(ID))
			{
				OUT.insert(ID, P.value(ID));
			}

			pointLayers.insert(Table.Name, OUT);
		}
	}

	return pointLayers;
}

QHash<QString, QHash<int, QString>> DatabaseDriver::loadTextLayers(const QList<TABLE>& Tabs, bool Hide)
{
	if (!Database.isOpen()) return QHash<QString, QHash<int, QString>>();

	QSqlQuery Query(Database); Query.setForwardOnly(true);
	QHash<QString, QHash<int, QString>> textLayers; QSet<int> Used;

	if (Hide)
	{
		Query.prepare(
			"SELECT DISTINCT "
				"P.ID_WARSTWY "
			"FROM "
				"EW_TEXT P "
			"WHERE "
				"P.STAN_ZMIANY = 0 AND "
				"P.TYP <> 4 AND "
				"P.ID NOT IN ("
					"SELECT "
						"E.IDE "
					"FROM "
						"EW_OB_ELEMENTY E "
					"INNER JOIN "
						"EW_OBIEKTY O "
					"ON "
						"O.UID = E.UIDO "
					"WHERE "
						"O.STATUS = 0 AND "
						"E.TYP = 0"
				")");

		if (Query.exec()) while (Query.next()) Used.insert(Query.value(0).toInt());
	}

	for (const auto& Table : Tabs)
	{
		QHash<int, QString> T, OUT;

		{
			Query.prepare(QString(
				"SELECT "
					"T.ID, G.NAZWA_L "
				"FROM "
					"EW_WARSTWA_TEXTOWA T "
				"INNER JOIN "
					"EW_GRUPY_WARSTW G "
				"ON "
					"T.ID_GRUPY = G.ID "
				"INNER JOIN "
					"EW_OB_KODY_OPISY O "
				"ON "
					"G.ID = O.ID_WARSTWY "
				"WHERE "
					"O.KOD = '%1' AND "
					"T.NAZWA = O.KOD AND "
					"G.NAZWA LIKE '%#_E' "
				"ESCAPE "
					"'#' "
				"ORDER BY "
					"G.NAZWA_L")
					    .arg(Table.Name));

			if (Query.exec()) while (Query.next())
			{
				T.insert(Query.value(0).toInt(), Query.value(1).toString());
			}
		}

		if (T.isEmpty())
		{
			Query.prepare(QString(
				"SELECT "
					"T.ID, T.DLUGA_NAZWA "
				"FROM "
					"EW_WARSTWA_TEXTOWA T "
				"INNER JOIN "
					"EW_GRUPY_WARSTW G "
				"ON "
					"T.ID_GRUPY = G.ID "
				"INNER JOIN "
					"EW_OB_KODY_OPISY O "
				"ON "
					"G.ID = O.ID_WARSTWY "
				"WHERE "
					"O.KOD = '%1' AND "
					"T.NAZWA LIKE (SUBSTRING(O.KOD FROM 1 FOR 4) || '%') "
				"ORDER BY "
					"T.DLUGA_NAZWA")
					    .arg(Table.Name));

			if (Query.exec()) while (Query.next())
			{
				T.insert(Query.value(0).toInt(), Query.value(1).toString());
			}
		}

		if (T.isEmpty() || T.size() != T.values().toSet().size())
		{
			T.clear();

			Query.prepare(QString(
				"SELECT "
					"T.ID, G.NAZWA_L "
				"FROM "
					"EW_WARSTWA_TEXTOWA T "
				"INNER JOIN "
					"EW_GRUPY_WARSTW G "
				"ON "
					"T.ID_GRUPY = G.ID "
				"INNER JOIN "
					"EW_OB_KODY_OPISY O "
				"ON "
					"G.ID = O.ID_WARSTWY "
				"WHERE "
					"O.KOD = '%1' AND "
					"T.NAZWA LIKE (SUBSTRING(O.KOD FROM 1 FOR 4) || '%') "
				"ORDER BY "
					"G.NAZWA_L")
					    .arg(Table.Name));

			if (Query.exec()) while (Query.next())
			{
				T.insert(Query.value(0).toInt(), Query.value(1).toString());
			}
		}

		if (!Hide) textLayers.insert(Table.Name, T);
		else
		{
			for (const auto& ID : T.keys()) if (Used.contains(ID))
			{
				OUT.insert(ID, T.value(ID));
			}

			textLayers.insert(Table.Name, OUT);
		}
	}

	return textLayers;
}

QHash<int, QString> DatabaseDriver::allLineLayers(void)
{
	if (!Database.isOpen()) return QHash<int, QString>(); QHash<int, QString> List;

	QSqlQuery Query(Database); Query.setForwardOnly(true);

	Query.prepare("SELECT ID, DLUGA_NAZWA FROM EW_WARSTWA_LINIOWA");

	if (Query.exec()) while (Query.next())
	{
		List.insert(Query.value(0).toInt(), Query.value(1).toString());
	}

	return List;
}

QHash<int, QString> DatabaseDriver::allTextLayers(void)
{
	if (!Database.isOpen()) return QHash<int, QString>(); QHash<int, QString> List;

	QSqlQuery Query(Database); Query.setForwardOnly(true);

	Query.prepare("SELECT ID, DLUGA_NAZWA FROM EW_WARSTWA_TEXTOWA");

	if (Query.exec()) while (Query.next())
	{
		List.insert(Query.value(0).toInt(), Query.value(1).toString());
	}

	return List;
}

QHash<int, DatabaseDriver::LINE> DatabaseDriver::loadLines(int Layer, int Flags)
{
	if (!Database.isOpen()) return QHash<int, LINE>(); QHash<int, LINE> Lines;

	QSqlQuery Query(Database); Query.setForwardOnly(true);

	Query.prepare(
		"SELECT "
			"P.ID, P.OPERAT, "
			"P.P0_X, P.P0_Y, "
			"IIF(P.PN_X IS NULL, P.P1_X, P.PN_X), "
			"IIF(P.PN_Y IS NULL, P.P1_Y, P.PN_Y), "
			"P.TYP_LINII "
		"FROM "
			"EW_POLYLINE P "
		"WHERE "
			"P.STAN_ZMIANY = 0 AND "
			"P.ID_WARSTWY = :layer AND "
			"P.P1_FLAGS = :flag AND "
			"P.ID NOT IN ("
				"SELECT "
					"E.IDE "
				"FROM "
					"EW_OB_ELEMENTY E "
				"WHERE "
					"E.TYP = 0)");

	Query.bindValue(":layer", Layer);
	Query.bindValue(":flag", Flags);

	if (Query.exec()) while (Query.next()) Lines.insert(Query.value(0).toInt(),
	{
		Query.value(0).toInt(),
		Query.value(1).toInt(),
		0, QSet<int>(),
		Query.value(2).toDouble(),
		Query.value(3).toDouble(),
		Query.value(4).toDouble(),
		Query.value(5).toDouble(),
		Query.value(6).toInt()
	});

	if (Flags == 4) QtConcurrent::blockingMap(Lines, [] (LINE& Line) -> void
	{
		const double nX = (Line.X1 + Line.X2) / 2.0;
		const double nY = (Line.Y1 + Line.Y2) / 2.0;

		Line.Rad = qAbs(Line.X1 - nX);
		Line.X1 = Line.X2 = nX;
		Line.Y1 = Line.Y2 = nY;
	});
	else QtConcurrent::blockingMap(Lines, [] (LINE& Line) -> void
	{
		const double dx = Line.X1 - Line.X2;
		const double dy = Line.Y1 - Line.Y2;

		Line.Len = qSqrt(dx * dx + dy * dy);
	});

	return Lines;
}

QHash<int, DatabaseDriver::POINT> DatabaseDriver::loadPoints(int Layer, bool Symbol)
{
	if (!Database.isOpen()) return QHash<int, POINT>(); QHash<int, POINT> Points;

	QSqlQuery Query(Database); Query.setForwardOnly(true);

	Query.prepare(
		"SELECT "
			"P.ID, P.OPERAT, "
			"IIF(P.ODN_X IS NULL, P.POS_X, P.POS_X + P.ODN_X), "
			"IIF(P.ODN_Y IS NULL, P.POS_Y, P.POS_Y + P.ODN_Y), "
			"P.TEXT, P.TYP, P.KAT, "
			"IIF("
				"(P.TYP <> 6 AND P.ODN_X IS NULL AND P.ODN_Y IS NULL) OR "
				"(P.TYP = 6 AND BIN_AND(P.JUSTYFIKACJA, 32) <> 32), 0, 1"
			")"
		"FROM "
			"EW_TEXT P "
		"WHERE "
			"P.STAN_ZMIANY = 0 AND "
			"P.ID_WARSTWY = :layer AND "
			"P.ID NOT IN ("
				"SELECT "
					"E.IDE "
				"FROM "
					"EW_OB_ELEMENTY E "
				"WHERE "
					"E.TYP = 0)");

	Query.bindValue(":layer", Layer);

	if (Query.exec()) while (Query.next()) if (Symbol ? Query.value(5).toInt() == 4 : Query.value(5).toInt() != 4) Points.insert(Query.value(0).toInt(),
	{
		Query.value(0).toInt(),
		Query.value(1).toInt(),
		0,
		Query.value(2).toDouble(),
		Query.value(3).toDouble(),
		NAN,
		Query.value(6).toDouble(),
		Query.value(4).toString(),
		Query.value(7).toBool()
	});

	return Points;
}

QList<DatabaseDriver::OBJECT> DatabaseDriver::proceedLines(int Line, int Text, const QString& Expr, double Length, double Spin, int Job, bool Keep)
{
	if (!Database.isOpen()) return QList<OBJECT>();

	QHash<int, LINE> Lines; QHash<int, POINT> Points; QMutex Locker;
	QList<QPointF> Cuts; QList<OBJECT> Objects; QSet<int> Used;

	const auto getCuts = [&Lines, &Cuts] (void) -> void
	{
		const static auto append = [] (const QPointF& P, QList<QPointF>& Cords, QList<int>& Count, QList<QPointF>& Cuts) -> void
		{
			int i = Cords.indexOf(P);

			if (i == -1)
			{
				Cords.append(P);
				Count.append(1);
			}
			else if (++Count[i] == 3)
			{
				Cuts.append(P);
			}
		};

		QList<QPointF> Cords; QList<int> Count;

		for (const auto& Line : Lines)
		{
			const QPointF A(Line.X1, Line.Y1);
			const QPointF B(Line.X2, Line.Y2);

			append(A, Cords, Count, Cuts);
			append(B, Cords, Count, Cuts);
		}
	};

	const auto getPairs = [&Lines, &Expr, &Used, Length, Spin, Job] (POINT& P) -> void
	{
		static const auto length = [] (double x1, double y1, double x2, double y2)
		{
			const double dx = x1 - x2;
			const double dy = y1 - y2;

			return qSqrt(dx * dx + dy * dy);
		};

		if (Used.contains(P.ID)) return;

		if (!Expr.isEmpty()) if (QRegExp(Expr).indexIn(P.Text) == -1) return;

		for (const auto& L : Lines) if (!L.Label && (!(Job & 0b01) || !P.IDK || P.IDK == L.IDK || !L.IDK))
		{
			double dtg = P.FI + qAtan2(L.X1 - L.X2, L.Y1 - L.Y2);

			while (dtg >= M_PI) dtg -= M_PI; while (dtg < 0.0) dtg += M_PI;

			if (!P.Pointer && (qAbs(dtg) > Spin) && (qAbs(dtg - M_PI) > Spin)) continue;

			const double a = length(P.X, P.Y, L.X1, L.Y1);
			const double b = length(P.X, P.Y, L.X2, L.Y2);

			if ((a * a <= L.Len * L.Len + b * b) &&
			    (b * b <= L.Len * L.Len + a * a))
			{
				const double A = P.X - L.X1; const double B = P.Y - L.Y1;
				const double C = L.X2 - L.X1; const double D = L.Y2 - L.Y1;

				const double h = qAbs(A * D - C * B) / qSqrt(C * C + D * D);

				if (h <= Length && (qIsNaN(P.L) || h < P.L))
				{
					P.L = h; P.Match = L.ID;
				};
			}
		}
	};

	const auto getLabel = [&Points, &Used, &Locker, Job] (LINE& L) -> void
	{
		if (L.Label) return;

		for (const auto& P : Points) if (P.Match == L.ID && (qIsNaN(L.Odn) || L.Odn > P.L))
		{
			if (!(Job & 0b01) || !P.IDK || P.IDK == L.IDK || !L.IDK) { L.Label = P.ID; L.Odn = P.L; }
		}

		if (L.Label && !L.IDK && Points[L.Label].IDK)
		{
			L.IDK = Points[L.Label].IDK;
		}

		if (L.Label)
		{
			const QString& Label = Points[L.Label].Text; L.Labels.insert(L.Label);

			for (const auto& P : Points) if (P.Match == L.ID && P.Text == Label)
			{
				L.Labels.insert(P.ID);

				Locker.lock();
				Used.insert(P.ID);
				Locker.unlock();
			}
		}
	};

	const auto resetMatch = [] (POINT& P) -> void { P.Match = 0; P.L = NAN; };

	const auto getObjects = [&Lines, &Points, &Cuts, &Objects, Keep, Job] (void) -> void
	{
		const static auto append = [] (const LINE& L, OBJECT& O, const QHash<int, POINT>& P, QSet<int>& U, bool First) -> void
		{
			if (!O.IDK && L.IDK) O.IDK = L.IDK;

			if (L.Label)
			{
				if (O.Label.isEmpty())
				{
					O.Label = P[L.Label].Text;
				}

				O.Labels.append(L.Labels.toList());
			}

			if (First) O.Geometry.push_front(L.ID);
			else O.Geometry.push_back(L.ID);

			U.insert(L.ID);
		};

		QSet<int> Used; for (const auto& S : Lines) if (!Used.contains(S.ID))
		{
			QPointF P1(S.X1, S.Y1), P2(S.X2, S.Y2);
			OBJECT O; bool Continue = true;

			append(S, O, Points, Used, false);

			while (Continue)
			{
				const int oldSize = O.Geometry.size();

				for (const auto& L : Lines) if (!Used.contains(L.ID))
				{
					const QString Label = L.Label ? Points[L.Label].Text : QString();

					if ((S.Style == L.Style) && (!(Job & 0b10) || !L.IDK || !S.IDK || S.IDK == L.IDK))
					{
						QPointF L1(L.X1, L.Y1), L2(L.X2, L.Y2);

						int T(0); if (P1 == L1 && !Cuts.contains(P1)) T = 1;
						else if (P1 == L2 && !Cuts.contains(P1)) T = 2;
						else if (P2 == L1 && !Cuts.contains(P2)) T = 3;
						else if (P2 == L2 && !Cuts.contains(P2)) T = 4;

						if (T && (O.Label.isEmpty() || Label.isEmpty() || O.Label == Label))
						{
							switch (T)
							{
								case 1: P1 = L2; append(L, O, Points, Used, true); break;
								case 2: P1 = L1; append(L, O, Points, Used, true); break;
								case 3: P2 = L2; append(L, O, Points, Used, false); break;
								case 4: P2 = L1; append(L, O, Points, Used, false); break;
							}
						}
					}
				}

				Continue = oldSize != O.Geometry.size();
			}

			if (Keep || P1 != P2) Objects.append(O);
		}
	};

	for (const auto& L : loadLines(Line, 0)) Lines.insert(L.ID, L);
	for (const auto& L : loadLines(Line, 2)) Lines.insert(L.ID, L);

	if (Text) Points = loadPoints(Text, false);

	if (Lines.isEmpty()) return QList<OBJECT>();

	QFutureSynchronizer<void> Synchronizer;

	Synchronizer.addFuture(QtConcurrent::run(getCuts));

	bool Continue(true); do
	{
		const int Count = Used.size();

		QtConcurrent::blockingMap(Points, resetMatch);
		QtConcurrent::blockingMap(Points, getPairs);
		QtConcurrent::blockingMap(Lines, getLabel);

		Continue = Count != Used.size();
	}
	while (Continue);

	Synchronizer.waitForFinished();

	Synchronizer.addFuture(QtConcurrent::run(getObjects));

	Synchronizer.waitForFinished();

	return Objects;
}


QList<DatabaseDriver::OBJECT> DatabaseDriver::proceedPoints(int Symbol, int Text, const QString& Expr, double Length, int Job)
{
	if (!Database.isOpen()) return QList<OBJECT>();

	QHash<int, POINT> Symbols, Texts; QMutex Locker;
	QList<OBJECT> Objects; QSet<int> Used;

	const auto getPairs = [&Symbols, &Expr, &Used, Length, Job] (POINT& P) -> void
	{
		static const auto length = [] (double x1, double y1, double x2, double y2)
		{
			const double dx = x1 - x2;
			const double dy = y1 - y2;

			return qSqrt(dx * dx + dy * dy);
		};

		if (Used.contains(P.ID)) return;

		if (!Expr.isEmpty()) if (QRegExp(Expr).indexIn(P.Text) == -1) return;

		for (const auto& S : Symbols) if (!S.Match && (!(Job & 0b01) || !P.IDK || P.IDK == S.IDK || !S.IDK))
		{
			const double l = length(S.X, S.Y, P.X, P.Y);

			if (l <= Length && (qIsNaN(P.L) || l < P.L))
			{
				P.L = l; P.Match = S.ID;
			};
		}
	};

	const auto getLabel = [&Texts, &Used, &Locker, Job] (POINT& S) -> void
	{
		if (S.Match) return;

		for (const auto& P : Texts) if (P.Match == S.ID && (qIsNaN(S.L) || S.L > P.L))
		{
			if (!(Job & 0b01) || !P.IDK || P.IDK == S.IDK || !S.IDK) { S.Match = P.ID; S.L = P.L; }
		}

		if (S.Match && !S.IDK && Texts[S.Match].IDK)
		{
			S.IDK = Texts[S.Match].IDK;
		}

		if (S.Match)
		{
			Locker.lock();
			Used.insert(S.Match);
			Locker.unlock();
		}
	};

	const auto resetMatch = [] (POINT& P) -> void { P.Match = 0; P.L = NAN; };

	const auto getObjects = [&Symbols, &Objects, &Texts] (void) -> void
	{
		for (const auto& S : Symbols)
		{
			OBJECT O; O.IDK = S.IDK;
			O.Geometry.append(S.ID);

			if (S.Match)
			{
				O.Label = Texts[S.Match].Text;
				O.Labels.append(S.Match);
			}

			Objects.append(O);
		}
	};

	Symbols = loadPoints(Symbol, true);

	if (Symbols.isEmpty()) return QList<OBJECT>();
	else if (Text) Texts = loadPoints(Text, false);

	bool Continue(true); do
	{
		const int Count = Used.size();

		QtConcurrent::blockingMap(Texts, resetMatch);
		QtConcurrent::blockingMap(Texts, getPairs);
		QtConcurrent::blockingMap(Symbols, getLabel);

		Continue = Count != Used.size();
	}
	while (Continue);

	QFutureSynchronizer<void> Synchronizer;

	Synchronizer.addFuture(QtConcurrent::run(getObjects));

	Synchronizer.waitForFinished();

	return Objects;
}

QList<DatabaseDriver::OBJECT> DatabaseDriver::proceedSurfaces(int Line, int Text, const QString& Expr, double Length, int Job)
{
	if (!Database.isOpen()) return QList<OBJECT>();

	QHash<int, LINE> Lines; QHash<int, POINT> Points; QMutex Locker;
	QList<LINE> Sorted; QList<OBJECT> Objects; QSet<int> Used;

	const auto sortLines = [] (const QHash<int, LINE>& Lines) -> QList<LINE>
	{
		QHash<int, QList<LINE>> Groups; QList<LINE> Sorted;
		QList<QPointF> Points; QList<int> Counts;

		for (const auto& L : Lines)
		{
			QPointF P1(L.X1, L.Y1), P2(L.X2, L.Y2);

			const int N1 = Points.indexOf(P1);

			if (N1 != -1) ++Counts[N1];
			else
			{
				Points.append(P1);
				Counts.append(1);
			}

			const int N2 = Points.indexOf(P2);

			if (N2 != -1) ++Counts[N2];
			else
			{
				Points.append(P2);
				Counts.append(1);
			}
		}

		for (const auto& L : Lines)
		{
			QPointF P1(L.X1, L.Y1), P2(L.X2, L.Y2);

			const int N1 = Points.indexOf(P1);
			const int N2 = Points.indexOf(P2);

			const int N = Counts[N1] + Counts[N2];

			Groups[N].append(L);
		}

		QList<int> Keys = Groups.keys(); qSort(Keys);

		for (const auto& K : Keys) if (K > 1) Sorted.append(Groups[K]);

		return Sorted;
	};

	const auto getPairs = [&Objects, &Lines, &Expr, &Used, Length, Job] (POINT& P) -> void
	{
		static const auto length = [] (double x1, double y1, double x2, double y2)
		{
			const double dx = x1 - x2;
			const double dy = y1 - y2;

			return qSqrt(dx * dx + dy * dy);
		};

		if (Used.contains(P.ID)) return;

		if (!Expr.isEmpty()) if (QRegExp(Expr).indexIn(P.Text) == -1) return;

		for (const auto& S : Objects) if (!(Job & 0b01) || !S.IDK || P.IDK == S.IDK || !P.IDK)
		{
			QPolygonF Poly; Poly.reserve(S.Geometry.size() + 1);

			const auto& LF = Lines[S.Geometry.first()];
			const auto& LS = Lines[S.Geometry.last()];

			const QLineF First = { LF.X1, LF.Y1, LF.X2, LF.Y2 };
			const QLineF Last = { LS.X1, LS.Y1, LS.X2, LS.Y2 };

			if (First.p1() == Last.p1() || First.p1() == Last.p2())
			{
				Poly.append({ LF.X1, LF.Y1 });
			}
			else
			{
				Poly.append({ LF.X2, LF.Y2 });
			}

			for (const auto& PID : S.Geometry)
			{
				const auto& Part = Lines[PID];

				const QPointF P1(Part.X1, Part.Y1);
				const QPointF P2(Part.X1, Part.Y1);

				if (!Poly.contains(P1)) Poly.append(P1);
				if (!Poly.contains(P2)) Poly.append(P2);
			}

			if (Poly.size() > 2 && Poly.containsPoint({ P.X, P.Y }, Qt::OddEvenFill))
			{
				P.L = 0.0; P.Match = Lines[S.Geometry.first()].ID;
			}
			else for (const auto& PID: S.Geometry)
			{
				const auto& L = Lines[PID];

				double h(NAN); if (qIsNaN(L.Rad))
				{
					const double a = length(P.X, P.Y, L.X1, L.Y1);
					const double b = length(P.X, P.Y, L.X2, L.Y2);

					if ((a * a <= L.Len * L.Len + b * b) &&
					    (b * b <= L.Len * L.Len + a * a))
					{
						const double A = P.X - L.X1; const double B = P.Y - L.Y1;
						const double C = L.X2 - L.X1; const double D = L.Y2 - L.Y1;

						h = qAbs(A * D - C * B) / qSqrt(C * C + D * D);
					}
				}
				else
				{
					h = length(P.X, P.Y, L.X1, L.Y1) - L.Rad;
				}

				if (!qIsNaN(h) && h <= Length && (qIsNaN(P.L) || h < P.L))
				{
					P.L = h; P.Match = L.ID;
				};
			}
		}
	};

	const auto getLabel = [&Points, Job] (LINE& L) -> void
	{
		for (const auto& P : Points) if (P.Match == L.ID && (qIsNaN(L.Odn) || L.Odn > P.L))
		{
			if (!(Job & 0b01) || !P.IDK || P.IDK == L.IDK || !L.IDK) { L.Label = P.ID; L.Odn = P.L; }
		}
	};

	const auto setLabel = [&Points, &Used, &Locker] (OBJECT& O) -> void
	{
		if (O.Labels.size()) return;

		double L = NAN; int Match = 0;

		for (const auto& P : Points) if (O.Geometry.contains(P.Match) && (qIsNaN(L) || L > P.L))
		{
			Match = P.ID; L = P.L;
		}

		if (Match && !O.IDK && Points[Match].IDK)
		{
			O.IDK = Points[Match].IDK;
		}

		if (Match)
		{
			O.Label = Points[Match].Text;
			O.Labels.append(Match);

			Locker.lock();
			Used.insert(Match);
			Locker.unlock();
		}
	};

	const auto resetMatch = [] (POINT& P) -> void { P.Match = 0; P.L = NAN; };

	const auto getObjects = [&Sorted, &Objects, &Lines, &Used, Job] (void) -> int
	{
		const static auto append = [] (const LINE& L, OBJECT& O, QSet<int>& U, QList<QPointF>& G, int T) -> void
		{
			if (!O.IDK && L.IDK) O.IDK = L.IDK;

			switch (T)
			{
				case 3:
					G.push_back(QPointF(L.X1, L.Y1));
				break;
				case 4:
					G.push_back(QPointF(L.X2, L.Y2));
				break;
				default:
					G.append(QPointF(L.X2, L.Y2));
			}

			O.Geometry.append(L.ID);
			U.insert(L.ID);
		};

		QList<int> Unused; const int Now = Used.size();

		for (const auto& S : Sorted) if (!Used.contains(S.ID))
		{
			QPointF P1(S.X1, S.Y1), P2(S.X2, S.Y2);
			OBJECT O; bool Continue = true;
			QList<QPointF> Geometry;

			append(S, O, Used, Geometry, 0);

			if (qIsNaN(S.Rad)) while (Continue)
			{
				const int oldSize = O.Geometry.size();

				for (const auto& L : Sorted) if (!Used.contains(L.ID))
				{
					if (!(Job & 0b10) || !L.IDK || !S.IDK || S.IDK == L.IDK)
					{
						QPointF L1(L.X1, L.Y1), L2(L.X2, L.Y2);

						int T(0); if (P2 == L1) T = 3; else if (P2 == L2) T = 4;

						int C(0); if (T) switch (T)
						{
							case 3: C = Geometry.indexOf(L2); break;
							case 4: C = Geometry.indexOf(L1); break;
						}

						if (T && C == -1)
						{
							append(L, O, Used, Geometry, T); switch (T)
							{
								case 3: P2 = L2; break; case 4: P2 = L1; break;
							}
						}

						if (T && C != -1) for (int i = 0, l = Geometry.size(); i < l - C; ++i)
						{
							Unused.append(O.Geometry.takeLast()); Geometry.removeLast();
						}
					}
				}

				Continue = oldSize != O.Geometry.size() && P1 != P2;
			}

			if ((P1 == P2 && O.Geometry.size() > 2) || !qIsNaN(S.Rad))
			{
				QList<QPointF> Breaks; QList<int> Counter; bool OK(true);

				if (qIsNaN(S.Rad)) for (const auto& Part : O.Geometry)
				{
					const auto& L = Lines[Part];

					QPointF P1(L.X1, L.Y1), P2(L.X2, L.Y2);

					const int N1 = Breaks.indexOf(P1);

					if (N1 != -1) ++Counter[N1];
					else
					{
						Breaks.append(P1);
						Counter.append(1);
					}

					const int N2 = Breaks.indexOf(P2);

					if (N2 != -1) ++Counter[N2];
					else
					{
						Breaks.append(P2);
						Counter.append(1);
					}
				}

				for (const auto& C : Counter) OK = OK && (C == 2);

				if (OK || !qIsNaN(S.Rad)) Objects.append(O);
				else for (const auto& I : O.Geometry) Used.remove(I);
			}
			else for (const auto& I : O.Geometry) Used.remove(I);

			while (Unused.size()) Used.remove(Unused.takeLast());
		}

		return Used.size() - Now;
	};

	for (const auto& L : loadLines(Line, 0)) Lines.insert(L.ID, L);
	for (const auto& L : loadLines(Line, 2)) Lines.insert(L.ID, L);
	for (const auto& L : loadLines(Line, 4)) Lines.insert(L.ID, L);

	Sorted = sortLines(Lines);

	if (Sorted.isEmpty()) return QList<OBJECT>();
	else if (Text) Points = loadPoints(Text, false);

	bool Continue; while (!isTerminated() && getObjects()) do
	{
		const int Count = Used.size();

		QtConcurrent::blockingMap(Points, resetMatch);
		QtConcurrent::blockingMap(Points, getPairs);
		QtConcurrent::blockingMap(Sorted, getLabel);
		QtConcurrent::blockingMap(Objects, setLabel);

		Continue = Count != Used.size();
	}
	while (Continue);

	return Objects;
}

QList<DatabaseDriver::OBJECT> DatabaseDriver::proceedTexts(int Text, int Point, const QString& Expr, int Fit, double Length, const QString& Symbol)
{
	if (!Database.isOpen()) return QList<OBJECT>(); QMutex Locker; QList<QPointF> Points;

	QHash<int, POINT> Texts = loadPoints(Text, false); QList<OBJECT> Objects; QList<POINT> Symbols;

	QSqlQuery symbolQuery(Database), insertQuery(Database), pointsQuery(Database);

	const auto getSymbols = [&Expr, &Symbol, &Symbols, &Points, &Locker, Fit, Length] (POINT& P) -> void
	{
		if (QRegExp(Expr).indexIn(P.Text) == -1) return;

		QPointF Found; if (Fit == 2)
		{
			double L = NAN; const QPointF T(P.X, P.Y);

			for (const auto S : Points)
			{
				const double l = QLineF(T, S).length();

				if (l <= Length && (qIsNaN(L) || l < L))
				{
					Found = S; L = l;
				}
			}
		}
		else if (Fit == 1) Found = QPointF(P.X, P.Y);

		if (Found != QPointF())
		{
			Locker.lock();
			Symbols.append(
			{
				0, P.IDK, P.ID,
				Found.x(), Found.y(),
				NAN, NAN, Symbol
			});
			Locker.unlock();
		}
	};

	const auto getObjects = [&Texts, &Objects, &Locker] (POINT& S) -> void
	{
		for (const auto& T : Texts) if (S.Match == T.ID)
		{
			Locker.lock();
			Objects.append(
			{
				QHash<int, QVariant>(),
				QList<int>() << S.ID,
				QList<int>() << T.ID,
				T.Text, S.IDK
			});
			Locker.unlock();

			return;
		}
	};

	symbolQuery.prepare("SELECT GEN_ID(EW_ELEMENT_UID_GEN, 1), GEN_ID(EW_ELEMENT_ID_GEN, 1) FROM RDB$DATABASE");

	insertQuery.prepare(QString(
		"INSERT INTO "
			"EW_TEXT (UID, ID, ID_WARSTWY, STAN_ZMIANY, TYP, TEXT, CREATE_TS, MODIFY_TS, POS_X, POS_Y) "
		"VALUES "
			"(?, ?, %1, 0, 4, '%2', CURRENT_TIMESTAMP, CURRENT_TIMESTAMP, ?, ?)")
				  .arg(Point)
				  .arg(Symbol));

	pointsQuery.prepare(
		"SELECT "
			"T.POS_X, T.POS_Y "
		"FROM "
			"EW_TEXT T "
		"WHERE "
			"T.STAN_ZMIANY = 0 AND "
			"T.TYP = 4 AND "
			"T.TEXT <> :class "
		"UNION "
		"SELECT "
			"F.P0_X, F.P0_Y "
		"FROM "
			"EW_POLYLINE F "
		"WHERE "
			"F.STAN_ZMIANY = 0 AND "
			"F.P1_FLAGS <> 4 "
		"UNION "
		"SELECT "
			"L.P1_X, L.P1_Y "
		"FROM "
			"EW_POLYLINE L "
		"WHERE "
			"L.STAN_ZMIANY = 0 AND "
			"L.P1_FLAGS = 0 "
		"UNION "
		"SELECT "
			"A.PN_X, A.PN_Y "
		"FROM "
			"EW_POLYLINE A "
		"WHERE "
			"A.STAN_ZMIANY = 0 AND "
			"A.P1_FLAGS = 2 "
		"UNION "
		"SELECT "
			"(C.P0_X + C.P1_X) / 2.0, "
			"(C.P0_Y + C.P1_Y) / 2.0 "
		"FROM "
			"EW_POLYLINE C "
		"WHERE "
			"C.STAN_ZMIANY = 0 AND "
			"C.P1_FLAGS = 4");

	pointsQuery.bindValue(":class", Symbol);

	if (Fit == 2 && pointsQuery.exec()) while (pointsQuery.next()) Points.append(
	{
		pointsQuery.value(0).toDouble(), pointsQuery.value(1).toDouble()
	});

	QtConcurrent::blockingMap(Texts, getSymbols);

	for (auto& S : Symbols) if (symbolQuery.exec() && symbolQuery.next())
	{
		const int UID = symbolQuery.value(0).toInt();
		const int IDE = symbolQuery.value(1).toInt();

		insertQuery.addBindValue(UID);
		insertQuery.addBindValue(IDE);
		insertQuery.addBindValue(S.X);
		insertQuery.addBindValue(S.Y);

		if (insertQuery.exec()) S.ID = IDE;
	}

	QtConcurrent::blockingMap(Symbols, getObjects);

	return Objects;
}

QHash<int, QVariant> DatabaseDriver::proceedGeometry(const QList<QPointF>& Points, double Radius, int Text, int Line, bool Emit)
{
	if (!Database.isOpen()) return QHash<int, QVariant>(); QMutex Locker; int Step(0);

	QList<QPair<int, QPointF>> Symbols;
	QList<QPair<int, QLineF>> Circles;
	QList<QPair<int, QLineF>> Lines;

	QHash<int, QVariant> Updates;

	QSqlQuery Query(Database); Query.setForwardOnly(true);

	Query.prepare(
		"SELECT "
			"P.ID, P.POS_X, P.POS_Y "
		"FROM "
			"EW_TEXT P "
		"WHERE "
			"P.STAN_ZMIANY = 0 AND "
			"P.TYP = 4 AND "
			"("
				":layer = -1 OR P.ID_WARSTWY = :layer"
			")");

	Query.bindValue(":layer", Text);

	if (Query.exec()) while (Query.next()) Symbols.append(
	{
		Query.value(0).toInt(),
		{
			Query.value(1).toDouble(),
			Query.value(2).toDouble()
		}
	});

	Query.prepare(
		"SELECT "
			"P.ID, P.P0_X, P.P0_Y, P.P1_X, P.P1_Y "
		"FROM "
			"EW_POLYLINE P "
		"WHERE "
			"P.STAN_ZMIANY = 0 AND "
			"P.P1_FLAGS = 4 AND "
			"("
				":layer = -1 OR P.ID_WARSTWY = :layer"
			")");

	Query.bindValue(":layer", Line);

	if (Query.exec()) while (Query.next()) Circles.append(
	{
		Query.value(0).toInt(),
		{
			{
				Query.value(1).toDouble(),
				Query.value(2).toDouble()
			},
			{
				Query.value(3).toDouble(),
				Query.value(4).toDouble()
			}
		}
	});

	Query.prepare(
		"SELECT "
			"P.ID, P.P0_X, P.P0_Y, "
			"IIF(P.PN_X IS NULL, P.P1_X, P.PN_X), "
			"IIF(P.PN_Y IS NULL, P.P1_Y, P.PN_Y) "
		"FROM "
			"EW_POLYLINE P "
		"WHERE "
			"P.STAN_ZMIANY = 0 AND "
			"P.P1_FLAGS IN (0, 2) AND "
			"("
				":layer = -1 OR P.ID_WARSTWY = :layer"
			")");

	Query.bindValue(":layer", Line);

	if (Query.exec()) while (Query.next()) Lines.append(
	{
		Query.value(0).toInt(),
		{
			{
				Query.value(1).toDouble(),
				Query.value(2).toDouble()
			},
			{
				Query.value(3).toDouble(),
				Query.value(4).toDouble()
			}
		}
	});

	if (Emit) emit onSetupProgress(0, Symbols.size() + Circles.size() + Lines.size());

	QtConcurrent::blockingMap(Symbols, [this, &Points, &Updates, &Locker, &Step, Radius, Emit] (QPair<int, QPointF>& S) -> void
	{
		double L = NAN; QPointF Found;

		for (const auto& P : Points) if (P != S.second)
		{
			const double l = QLineF(S.second, P).length();

			if (l <= Radius && (qIsNaN(L) || L > l))
			{
				L = l; Found = P;
			}
		}

		if (Found != QPointF())
		{
			Locker.lock();
			Updates.insert(S.first, Found);
			Locker.unlock();
		}

		if (Emit)
		{
			Locker.lock();
			emit onUpdateProgress(++Step);
			Locker.unlock();
		}
	});

	QtConcurrent::blockingMap(Circles, [this, &Points, &Updates, &Locker, &Step, Radius, Emit] (QPair<int, QLineF>& S) -> void
	{
		double L = NAN; QPointF Found; QPointF C =
		{
			(S.second.x1() + S.second.x2()) / 2.0,
			(S.second.y1() + S.second.y2()) / 2.0
		};

		const double R = qAbs(S.second.x1() - S.second.x2()) / 2.0;

		for (const auto& P : Points) if (P != C)
		{
			const double l = QLineF(C, P).length();

			if (l <= Radius && (qIsNaN(L) || L > l))
			{
				L = l; Found = P;
			}
		}

		if (Found != QPointF())
		{
			QLineF Updated(Found.x() - R, Found.y(),
						Found.x() + R, Found.y());

			Locker.lock();
			Updates.insert(S.first, Updated);
			Locker.unlock();
		}

		if (Emit)
		{
			Locker.lock();
			emit onUpdateProgress(++Step);
			Locker.unlock();
		}
	});

	QtConcurrent::blockingMap(Lines, [this, &Points, &Updates, &Locker, &Step, Radius, Emit] (QPair<int, QLineF>& S) -> void
	{
		double L1 = NAN, L2 = NAN; QPointF Found1, Found2;

		for (const auto& P : Points) if (P != S.second.p1() || P != S.second.p2())
		{
			const double l1 = QLineF(S.second.p1(), P).length();
			const double l2 = QLineF(S.second.p2(), P).length();

			if (l1 <= Radius && (qIsNaN(L1) || L1 > l1))
			{
				L1 = l1; Found1 = P;
			}

			if (l2 <= Radius && (qIsNaN(L2) || L2 > l2))
			{
				L2 = l2; Found2 = P;
			}
		}

		if (Found1 == Found2) return;
		else if (Found1 != QPointF() || Found2 != QPointF())
		{
			QLineF Updated = S.second;

			if (Found1 != QPointF()) Updated.setP1(Found1);
			if (Found2 != QPointF()) Updated.setP2(Found2);

			Locker.lock();
			Updates.insert(S.first, Updated);
			Locker.unlock();
		}

		if (Emit)
		{
			Locker.lock();
			emit onUpdateProgress(++Step);
			Locker.unlock();
		}
	});

	return Updates;
}

int DatabaseDriver::proceedTextDuplicates(int Action, int Strategy, int Heurstic, int Layer, int Sublayer, double Radius)
{
	struct SEGMENT
	{
		int ID; QString Text;

		QPointF Geometry;
		QString Kerg;
		QDateTime Mod;

		bool Objected = false;
		unsigned Timestamp = 0;
	};

	if (!Database.open()) return 0; int Step = 0;

	QList<SEGMENT> Segments; QSet<int> Deletes; QMutex Synchronizer;

	QSqlQuery selectQuery(Database); selectQuery.setForwardOnly(true);
	QSqlQuery deleteQuery(Database); deleteQuery.setForwardOnly(true);

	emit onBeginProgress(tr("Loading elements"));
	emit onSetupProgress(0, 0);

	selectQuery.prepare(
		"SELECT "
			"T.UID, T.TEXT, T.POS_X, T.POS_Y, O.NUMER, T.MODIFY_TS, "
			"IIF(T.ID IN "
			"( "
				"SELECT E.IDE "
				"FROM EW_OB_ELEMENTY E "
				"INNER JOIN EW_OBIEKTY B "
				"ON E.UIDO = B.UID "
				"WHERE B.STATUS = 0 "
			"), 1, 0) "
		"FROM "
			"EW_TEXT T "
		"INNER JOIN "
			"EW_WARSTWA_TEXTOWA W "
		"ON "
			"T.ID_WARSTWY = W.ID "
		"INNER JOIN "
			"EW_GRUPY_WARSTW G "
		"ON "
			"W.ID_GRUPY = G.ID "
		"LEFT JOIN "
			"EW_OPERATY O "
		"ON "
			"T.OPERAT = O.UID "
		"WHERE "
			"T.STAN_ZMIANY = 0 AND "
			"( "
				":sublayer = W.ID OR :sublayer = 0 "
			") "
			"AND "
			"( "
				":layer = G.ID OR :layer = 0 "
			")");

	selectQuery.bindValue(":layer", Strategy < 2 ? Layer : 0);
	selectQuery.bindValue(":sublayer", Strategy < 1 ? Sublayer : 0);

	if (selectQuery.exec()) while (selectQuery.next())
	{
		const int UID = selectQuery.value(0).toInt();

		Segments.append(
		{
			UID,
			selectQuery.value(1).toString().trimmed(),
			{
				selectQuery.value(2).toDouble(),
				selectQuery.value(3).toDouble()
			},
			selectQuery.value(4).toString().trimmed(),
			selectQuery.value(5).toDateTime(),
			selectQuery.value(6).toBool()
		});
	}

	emit onBeginProgress(tr("Processing items"));
	emit onSetupProgress(0, 0);

	if (!Heurstic) QtConcurrent::blockingMap(Segments, [] (SEGMENT& Seg) -> void
	{
		QRegExp Exp("(\\d+)?[\\.\\,\\;\\/](\\d+)$");

		if (Exp.indexIn(Seg.Kerg) != -1)
		{
			bool aOK, bOK; int A, B; unsigned Hash(0);

			A = Exp.capturedTexts()[1].toInt(&aOK);
			B = Exp.capturedTexts()[2].toInt(&bOK);

			if (bOK && B < 100) B += 1900;

			if (bOK) Hash += B * 10000;
			if (aOK) Hash += A;

			if (Hash) Seg.Timestamp = Hash;
		}
	});
	else QtConcurrent::blockingMap(Segments, [] (SEGMENT& Seg) -> void
	{
		Seg.Timestamp = Seg.Mod.toSecsSinceEpoch();
	});

	if (!Action) QtConcurrent::blockingMap(Segments, [&Segments, &Deletes, &Synchronizer, Radius] (SEGMENT& Seg) -> void
	{
		for (const auto& Other : Segments) if (!Seg.Objected && Seg.ID != Other.ID && Seg.Timestamp <= Other.Timestamp)
		{
			if (QLineF(Seg.Geometry, Other.Geometry).length() <= Radius)
			{
				Synchronizer.lock();

				if (!Deletes.contains(Other.ID)) Deletes.insert(Seg.ID);

				Synchronizer.unlock();
			}
		}
	});
	else QtConcurrent::blockingMap(Segments, [&Segments, &Deletes, &Synchronizer, Radius] (SEGMENT& Seg) -> void
	{
		for (const auto& Other : Segments) if (!Seg.Objected && Other.Objected && Seg.ID != Other.ID)
		{
			if (QLineF(Seg.Geometry, Other.Geometry).length() <= Radius)
			{
				Synchronizer.lock();

				if (!Deletes.contains(Other.ID)) Deletes.insert(Seg.ID);

				Synchronizer.unlock();
			}
		}
	});

	emit onBeginProgress(tr("Removing items"));
	emit onSetupProgress(0, 0);

	deleteQuery.prepare("DELETE FROM EW_TEXT WHERE UID = ?");

	for (const auto& ID : Deletes)
	{
		deleteQuery.addBindValue(ID);
		deleteQuery.exec();

		emit onUpdateProgress(++Step);
	}

	return Deletes.size();
}

int DatabaseDriver::proceedLineDuplicates(int Action, int Strategy, int Heurstic, int Layer, int Sublayer, double Radius)
{
	struct SEGMENT
	{
		int ID;

		QLineF Geometry;
		QString Kerg;
		QDateTime Mod;

		bool Objected = false;
		unsigned Timestamp = 0;
	};

	if (!Database.open()) return 0; int Step = 0;

	QList<SEGMENT> Segments; QSet<int> Deletes; QMutex Synchronizer;

	QSqlQuery selectQuery(Database); selectQuery.setForwardOnly(true);
	QSqlQuery deleteQuery(Database); deleteQuery.setForwardOnly(true);

	emit onBeginProgress(tr("Loading elements"));
	emit onSetupProgress(0, 0);

	selectQuery.prepare(
		"SELECT "
			"T.UID, T.P0_X, T.P0_Y, "
			"IIF(T.PN_X IS NULL, T.P1_X, T.PN_X), "
			"IIF(T.PN_Y IS NULL, T.P1_Y, T.PN_Y), "
			"O.NUMER, T.MODIFY_TS, IIF(T.ID IN "
			"( "
				"SELECT E.IDE "
				"FROM EW_OB_ELEMENTY E "
				"INNER JOIN EW_OBIEKTY B "
				"ON E.UIDO = B.UID "
				"WHERE B.STATUS = 0 "
			"), 1, 0) "
		"FROM "
			"EW_POLYLINE T "
		"INNER JOIN "
			"EW_WARSTWA_LINIOWA W "
		"ON "
			"T.ID_WARSTWY = W.ID "
		"INNER JOIN "
			"EW_GRUPY_WARSTW G "
		"ON "
			"W.ID_GRUPY = G.ID "
		"LEFT JOIN "
			"EW_OPERATY O "
		"ON "
			"T.OPERAT = O.UID "
		"WHERE "
			"T.STAN_ZMIANY = 0 AND "
			"( "
				":sublayer = W.ID OR :sublayer = 0 "
			") "
			"AND "
			"( "
				":layer = G.ID OR :layer = 0 "
			")");

	selectQuery.bindValue(":layer", Strategy < 2 ? Layer : 0);
	selectQuery.bindValue(":sublayer", Strategy < 1 ? Sublayer : 0);

	if (selectQuery.exec()) while (selectQuery.next())
	{
		const int UID = selectQuery.value(0).toInt();

		Segments.append(
		{
			UID,
			{
				selectQuery.value(1).toDouble(),
				selectQuery.value(2).toDouble(),
				selectQuery.value(3).toDouble(),
				selectQuery.value(4).toDouble()
			},
			selectQuery.value(5).toString().trimmed(),
			selectQuery.value(6).toDateTime(),
			selectQuery.value(7).toBool()
		});
	}

	emit onBeginProgress(tr("Processing items"));
	emit onSetupProgress(0, 0);

	if (!Heurstic) QtConcurrent::blockingMap(Segments, [] (SEGMENT& Seg) -> void
	{
		QRegExp Exp("(\\d+)?[\\.\\,\\;\\/](\\d+)$");

		if (Exp.indexIn(Seg.Kerg) != -1)
		{
			bool aOK, bOK; int A, B; unsigned Hash(0);

			A = Exp.capturedTexts()[1].toInt(&aOK);
			B = Exp.capturedTexts()[2].toInt(&bOK);

			if (bOK && B < 100) B += 1900;

			if (bOK) Hash += B * 10000;
			if (aOK) Hash += A;

			if (Hash) Seg.Timestamp = Hash;
		}
	});
	else QtConcurrent::blockingMap(Segments, [] (SEGMENT& Seg) -> void
	{
		Seg.Timestamp = Seg.Mod.toSecsSinceEpoch();
	});

	if (!Action) QtConcurrent::blockingMap(Segments, [&Segments, &Deletes, &Synchronizer, Radius] (SEGMENT& Seg) -> void
	{
		for (const auto& Other : Segments) if (!Seg.Objected && Seg.ID != Other.ID && Seg.Timestamp <= Other.Timestamp)
		{
			const double R1 = QLineF(Seg.Geometry.p1(), Other.Geometry.p1()).length();
			const double R2 = QLineF(Seg.Geometry.p2(), Other.Geometry.p2()).length();
			const double R3 = QLineF(Seg.Geometry.p2(), Other.Geometry.p1()).length();
			const double R4 = QLineF(Seg.Geometry.p1(), Other.Geometry.p2()).length();

			const int Count = (R1 <= Radius) + (R2 <= Radius) + (R3 <= Radius) + (R4 <= Radius);

			if (Count >= 2)
			{
				Synchronizer.lock();

				if (!Deletes.contains(Other.ID)) Deletes.insert(Seg.ID);

				Synchronizer.unlock();
			}
		}
	});
	else QtConcurrent::blockingMap(Segments, [&Segments, &Deletes, &Synchronizer, Radius] (SEGMENT& Seg) -> void
	{
		for (const auto& Other : Segments) if (!Seg.Objected && Other.Objected && Seg.ID != Other.ID)
		{
			const double R1 = QLineF(Seg.Geometry.p1(), Other.Geometry.p1()).length();
			const double R2 = QLineF(Seg.Geometry.p2(), Other.Geometry.p2()).length();
			const double R3 = QLineF(Seg.Geometry.p2(), Other.Geometry.p1()).length();
			const double R4 = QLineF(Seg.Geometry.p1(), Other.Geometry.p2()).length();

			const int Count = (R1 <= Radius) + (R2 <= Radius) + (R3 <= Radius) + (R4 <= Radius);

			if (Count >= 2)
			{
				Synchronizer.lock();

				if (!Deletes.contains(Other.ID)) Deletes.insert(Seg.ID);

				Synchronizer.unlock();
			}
		}
	});

	emit onBeginProgress(tr("Removing items"));
	emit onSetupProgress(0, 0);

	deleteQuery.prepare("DELETE FROM EW_POLYLINE WHERE UID = ?");

	for (const auto& ID : Deletes)
	{
		deleteQuery.addBindValue(ID);
		deleteQuery.exec();

		emit onUpdateProgress(++Step);
	}

	return Deletes.size();
}

bool DatabaseDriver::isTerminated(void) const
{
	QMutexLocker Locker(&Terminator); return Terminated;
}

bool DatabaseDriver::openDatabase(const QString& Server, const QString& Base, const QString& User, const QString& Pass)
{
	if (Database.isOpen()) Database.close();

	Database.setHostName(Server);
	Database.setDatabaseName(Base);
	Database.setUserName(User);
	Database.setPassword(Pass);

	if (Database.open())
	{
		emit onBeginProgress(tr("Loading database informations")); emit onLogin(true);

		Common = loadCommon(false);
		Tables = loadTables(true);

		Fields = normalizeFields(Tables, Common);
		Headers = normalizeHeaders(Tables, Common);

		emit onConnect(Tables, Common.size(),
					loadLineLayers(Tables, Hideempty),
					loadPointLayers(Tables, Hideempty),
					loadTextLayers(Tables, Hideempty),
					loadLayers(0), loadLayers(1),
					allLineLayers(), allTextLayers());

		emit onEndProgress();
	}
	else
	{
		emit onError(Database.lastError().text()); emit onLogin(false);
	}

	return Database.isOpen();
}

bool DatabaseDriver::closeDatabase(void)
{
	if (Database.isOpen())
	{
		Database.close(); emit onDisconnect(); return true;
	}
	else
	{
		emit onError(tr("Database is not opened")); return false;
	}
}

void DatabaseDriver::proceedClass(const QHash<int, QVariant>& Values, const QString& Pattern, const QString& Class, int Line, int Point, int Text, double Length, double Spin, bool Keep, int Job, int Snap, const QString& Insert)
{
	if (!Database.open()) { emit onProceedEnd(0); return; } QStringList Labels; int Step(0), Added(0);

	QSqlQuery indexQuery(Database), objectQuery(Database), dataQuery(Database), geometryQuery(Database);

	Terminator.lock(); Terminated = false; Terminator.unlock();

	const auto getValues = [&Values, &Pattern] (OBJECT& O) -> void
	{
		QRegExp Exp(Pattern); O.Values = Values;

		if (!Pattern.isEmpty() && Exp.indexIn(O.Label) != -1) for (int i = 0; i < Exp.captureCount(); ++i)
		{
			const QRegExp Var = QRegExp(QString("\\$%1\\b").arg(i + 1));

			for (auto& V : O.Values) if (V.type() == QVariant::String)
			{
				V = V.toString().replace(Var, Exp.capturedTexts()[i + 1]);
			}
		}

		for (auto& V : O.Values) if (V.type() == QVariant::String)
		{
			V = V.toString().remove(QRegExp("\\$\\d+\\b"));
		}
	};

	const auto insertObjects = [&] (const QList<OBJECT>& List, int Type) -> void
	{
		emit onBeginProgress(tr("Creating objects"));
		emit onSetupProgress(0, List.size()); Step = 0;

		for (const auto& O : List) if (!isTerminated())
		{
			if (indexQuery.exec() && indexQuery.next())
			{
				const QVariant ID = indexQuery.value(0); int n(0);

				const QString Numer = QString("%1%2")
								  .arg(qHash(QDateTime::currentMSecsSinceEpoch()), 0, 16)
								  .arg(qHash(ID.toInt()), 0, 16);

				objectQuery.addBindValue(ID);
				objectQuery.addBindValue(Numer);
				objectQuery.addBindValue(Type);
				objectQuery.addBindValue(O.IDK);

				objectQuery.exec();

				if (O.Values.size())
				{
					dataQuery.addBindValue(ID);

					for (const auto& V : O.Values) dataQuery.addBindValue(V);

					dataQuery.exec();
				}

				for (const auto& IDE : O.Geometry)
				{
					geometryQuery.addBindValue(ID);
					geometryQuery.addBindValue(n++);
					geometryQuery.addBindValue(IDE);

					geometryQuery.exec();
				}

				for (const auto& IDE : O.Labels)
				{
					geometryQuery.addBindValue(ID);
					geometryQuery.addBindValue(n++);
					geometryQuery.addBindValue(IDE);

					geometryQuery.exec();
				}

				Added += 1;
			}

			emit onUpdateProgress(++Step);
		}
	};

	const auto& Table = getItemByField(Tables, Class, &TABLE::Name);

	for (auto i = Values.constBegin(); i != Values.constEnd(); ++i)
	{
		Labels.append(Table.Fields[i.key()].Name); Labels.last().remove("EW_DATA.");
	}

	indexQuery.prepare("SELECT GEN_ID(EW_OBIEKTY_UID_GEN, 1) FROM RDB$DATABASE");

	objectQuery.prepare(QString(
		"INSERT INTO "
			"EW_OBIEKTY (UID, NUMER, IDKATALOG, KOD, RODZAJ, OPERAT, OSOU, OSOW, DTU, DTW, STATUS) "
		"VALUES "
			"(?, 'OB_ID_' || ?, 1, '%1', ?, ?, 0, 0, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP, 0)")
					.arg(Table.Name));

	if (Values.size()) dataQuery.prepare(QString(
		"INSERT INTO %1 (UIDO, %2) VALUES (?%3)")
				   .arg(Table.Data)
				   .arg(Labels.join(", "))
				   .arg(QString(", ?").repeated(Labels.size())));

	geometryQuery.prepare(
		"INSERT INTO "
			"EW_OB_ELEMENTY (UIDO, N, TYP, IDE) "
		"VALUES "
			"(?, ?, 0, ?)");

	if (Line && (Table.Flags & 0x002))
	{
		emit onBeginProgress(tr("Preparing surfaces"));
		emit onSetupProgress(0, 0);

		auto Objects = proceedSurfaces(Line, Text, Pattern, Length, Job);

		QtConcurrent::blockingMap(Objects, getValues); insertObjects(Objects, 3);
	}

	if (Point && (Table.Flags & 0x100))
	{
		emit onBeginProgress(tr("Preparing points"));
		emit onSetupProgress(0, 0);

		auto Objects = proceedPoints(Point, Text, Pattern, Length, Job);

		QtConcurrent::blockingMap(Objects, getValues); insertObjects(Objects, 4);
	}

	if (Line && (Table.Flags & 0x008))
	{
		emit onBeginProgress(tr("Preparing lines"));
		emit onSetupProgress(0, 0);

		auto Objects = proceedLines(Line, Text, Pattern, Length, Spin, Job, Keep);

		QtConcurrent::blockingMap(Objects, getValues); insertObjects(Objects, 2);
	}

	if (Text && Snap && (Table.Flags & 0x100))
	{
		emit onBeginProgress(tr("Preparing texts"));
		emit onSetupProgress(0, 0);

		auto Objects = proceedTexts(Text, Point, Pattern, Snap, Length, Insert);

		QtConcurrent::blockingMap(Objects, getValues); insertObjects(Objects, 4);
	}

	if (Hideempty) emit onReload(loadLineLayers(Tables, true),
						    loadPointLayers(Tables, true),
						    loadTextLayers(Tables, true));

	emit onEndProgress();
	emit onProceedEnd(Added);
}

void DatabaseDriver::proceedJobs(const QString& Path, const QString& Sep, int xPos, int yPos, int jobPos)
{
	if (!Database.open()) { emit onProceedEnd(0); return; }

	QFile File(Path); File.open(QFile::ReadOnly | QFile::Text);

	if (!File.isOpen()) { emit onProceedEnd(0); return; }

	struct DATA { double X, Y; QString Kerg; int ID = 0; };

	QTextStream Stream(&File); QList<DATA> Data;

	const bool CSV = (QFileInfo(File).suffix() == "csv");

	const int Max = qMax(qMax(xPos, yPos), jobPos);

	const QRegExp Exp(CSV ? "," : "\\s+");

	emit onBeginProgress(tr("Loading file"));
	emit onSetupProgress(0, 0);

	while (!Stream.atEnd())
	{
		DATA Item; const QStringList Items = Stream.readLine().split(Exp, QString::SkipEmptyParts);

		if (Max < Items.size())
		{
			Item.Kerg = Items[jobPos];
			Item.X = Items[xPos].toDouble();
			Item.Y = Items[yPos].toDouble();
		}
		else continue;

		if (!Sep.isEmpty()) Item.Kerg.truncate(Item.Kerg.lastIndexOf(Sep)); Data.append(Item);
	}

	emit onBeginProgress(tr("Loading items"));
	emit onSetupProgress(0, 0);

	QSqlQuery Query(Database); Query.setForwardOnly(true);

	QHash<int, QString> Kergs; int Step(0), Updates(0);
	QList<POINT> Points; QList<LINE> Lines; QMutex Locker;

	Query.prepare(
		"SELECT "
			"P.ID, "
			"ROUND(IIF(P.ODN_X IS NULL, P.POS_X, P.POS_X + P.ODN_X), 2), "
			"ROUND(IIF(P.ODN_Y IS NULL, P.POS_Y, P.POS_Y + P.ODN_Y), 2) "
		"FROM "
			"EW_TEXT P "
		"WHERE "
			"(P.OPERAT IS NULL OR P.OPERAT = 0) AND "
			"P.STAN_ZMIANY = 0 AND "
			"P.TYP = 4 AND "
			"P.ID NOT IN ("
				"SELECT "
					"E.IDE "
				"FROM "
					"EW_OB_ELEMENTY E "
				"WHERE "
					"E.TYP = 0)");

	if (Query.exec()) while (Query.next()) Points.append(
	{
		Query.value(0).toInt(),
		0, 0,
		Query.value(1).toDouble(),
		Query.value(2).toDouble(),
		NAN, NAN, QString()
	});

	Query.prepare(
		"SELECT "
			"P.ID, "
			"ROUND(P.P0_X, 2), "
			"ROUND(P.P0_Y, 2), "
			"ROUND(P.P1_X, 2), "
			"ROUND(P.P1_Y, 2) "
		"FROM "
			"EW_POLYLINE P "
		"WHERE "
			"(P.OPERAT IS NULL OR P.OPERAT = 0) AND "
			"P.STAN_ZMIANY = 0 AND "
			"P.ID NOT IN ("
				"SELECT "
					"E.IDE "
				"FROM "
					"EW_OB_ELEMENTY E "
				"WHERE "
					"E.TYP = 0)");

	if (Query.exec()) while (Query.next()) Lines.append(
	{
		Query.value(0).toInt(),
		0, 0, QSet<int>(),
		Query.value(1).toDouble(),
		Query.value(2).toDouble(),
		Query.value(3).toDouble(),
		Query.value(4).toDouble(),
		0
	});

	Query.prepare("SELECT K.UID, K.NUMER FROM EW_OPERATY K");

	if (Query.exec()) while (Query.next()) Kergs.insert(Query.value(0).toInt(),
											  Query.value(1).toString());

	emit onBeginProgress(tr("Preparing jobs"));
	emit onSetupProgress(0, Data.size()); Step = 0;

	for (auto& Item : Data)
	{
		for (auto i = Kergs.constBegin(); i != Kergs.constEnd(); ++i) if (!Item.ID)
		{
			if (Item.Kerg == i.value()) Item.ID = i.key();
		}

		if (Item.ID == 0)
		{
			QSqlQuery Get(Database); Get.setForwardOnly(true); int UID(0);

			Get.prepare("SELECT GEN_ID(EW_OPERATY_UID_GEN, 1) FROM RDB$DATABASE");

			if (Get.exec() && Get.next()) UID = Get.value(0).toInt(); else continue;

			Get.prepare("INSERT INTO EW_OPERATY (UID, NUMER, TYP, DTU, DTW) "
					  "VALUES (?, ?, 7, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)");

			Get.addBindValue(UID);
			Get.addBindValue(Item.Kerg);

			if (Get.exec()) { Item.ID = UID; Kergs.insert(UID, Item.Kerg); }
		}

		emit onUpdateProgress(++Step);
	}

	emit onBeginProgress(tr("Processing items")); Step = 0;
	emit onSetupProgress(0, Lines.size() + Points.size());

	QtConcurrent::blockingMap(Points, [this, &Data, &Updates, &Locker, &Step] (POINT& Point) -> void
	{
		bool OK(false); for (const auto& Item : Data) if (!OK) if (Item.X == Point.X && Item.Y == Point.Y)
		{
			QSqlQuery Update(Database); Update.setForwardOnly(true);

			Update.prepare("UPDATE EW_TEXT SET OPERAT = ? WHERE ID = ?");

			Update.addBindValue(Point.ID);
			Update.addBindValue(Item.ID);

			Update.exec();

			Locker.lock();
			Updates += 1;
			Locker.unlock();

			OK = true;
		}

		Locker.lock(); emit onUpdateProgress(++Step); Locker.unlock();
	});

	QtConcurrent::blockingMap(Lines, [this, &Data, &Updates, &Locker, &Step] (LINE& Line) -> void
	{
		QSet<int> One, Two; int UID(0);

		for (const auto& Item : Data)
		{
			if (Item.X == Line.X1 && Item.Y == Line.Y1) One.insert(Item.ID);
			else if (Item.X == Line.X2 && Item.Y == Line.Y2) Two.insert(Item.ID);
		}

		auto And = One & Two; if (And.size()) UID = *And.begin();

		if (UID != 0)
		{
			QSqlQuery Update(Database); Update.setForwardOnly(true);

			Update.prepare("UPDATE EW_TEXT SET OPERAT = ? WHERE ID = ?");

			Update.addBindValue(Line.ID);
			Update.addBindValue(UID);

			Update.exec();

			Locker.lock();
			Updates += 1;
			Locker.unlock();
		}

		Locker.lock(); emit onUpdateProgress(++Step); Locker.unlock();
	});

	emit onEndProgress();
	emit onProceedEnd(Updates);
}

void DatabaseDriver::proceedFit(const QString& Path, int xPos, int yPos, double Radius, int textLayer, int lineLayer)
{
	if (!Database.open()) { emit onProceedEnd(0); return; } QList<QPointF> Points; int Step(0);

	emit onBeginProgress(tr("Loading points"));
	emit onSetupProgress(0, 0);

	Terminator.lock(); Terminated = false; Terminator.unlock();

	if (!Path.isEmpty())
	{
		QFile File(Path); File.open(QFile::ReadOnly | QFile::Text); QTextStream Stream(&File);

		const bool CSV = (QFileInfo(File).suffix() == "csv");
		const int Max = qMax(--xPos, --yPos);

		const QRegExp Exp(CSV ? "," : "\\s+");

		while (!Stream.atEnd())
		{
			const QStringList Items = Stream.readLine().split(Exp, QString::SkipEmptyParts);

			if (Max < Items.size())
			{
				Points.append({ Items[xPos].toDouble(), Items[yPos].toDouble() });
			}
			else continue;
		}
	}
	else
	{
		QSqlQuery pointsQuery(Database); QList<QPointF> Base; QMutex Locker;
		QList<QPair<QPointF*, QSet<QPointF*>>> Pools; QSet<QPointF*> Rem; int Step(0);

		const auto getList = [this, &Pools, &Base, &Locker, &Step, Radius] (QPointF& I) -> void
		{
			QSet<QPointF*> List; for (auto& P : Base) if (&P != &I)
			{
				const double l = QLineF(I, P).length();

				if (l <= Radius) List.insert(&P);
			}

			QMutexLocker Synchronizer(&Locker); emit onUpdateProgress(++Step);

			for (int i = 0; i < Pools.size(); ++i)
			{
				if (List.size() >= Pools[i].second.size())
				{
					Pools.insert(i, qMakePair(&I, List)); return;
				}
			}

			Pools.append(qMakePair(&I, List));
		};

		pointsQuery.prepare(
			"SELECT "
				"T.POS_X, T.POS_Y "
			"FROM "
				"EW_TEXT T "
			"WHERE "
				"T.STAN_ZMIANY = 0 AND "
				"T.TYP = 4 AND "
				"( "
					":text = -1 OR T.ID_WARSTWY = :text "
				") "
			"UNION "
			"SELECT "
				"F.P0_X, F.P0_Y "
			"FROM "
				"EW_POLYLINE F "
			"WHERE "
				"F.STAN_ZMIANY = 0 AND "
				"F.P1_FLAGS <> 4 AND "
				"( "
					":line = -1 OR F.ID_WARSTWY = :line "
				") "
			"UNION "
			"SELECT "
				"L.P1_X, L.P1_Y "
			"FROM "
				"EW_POLYLINE L "
			"WHERE "
				"L.STAN_ZMIANY = 0 AND "
				"L.P1_FLAGS = 0 AND "
				"( "
					":line = -1 OR L.ID_WARSTWY = :line "
				") "
			"UNION "
			"SELECT "
				"A.PN_X, A.PN_Y "
			"FROM "
				"EW_POLYLINE A "
			"WHERE "
				"A.STAN_ZMIANY = 0 AND "
				"A.P1_FLAGS = 2 AND "
				"( "
					":line = -1 OR A.ID_WARSTWY = :line "
				") "
			"UNION "
			"SELECT "
				"(C.P0_X + C.P1_X) / 2.0, "
				"(C.P0_Y + C.P1_Y) / 2.0 "
			"FROM "
				"EW_POLYLINE C "
			"WHERE "
				"C.STAN_ZMIANY = 0 AND "
				"C.P1_FLAGS = 4 AND "
				"( "
					":line = -1 OR C.ID_WARSTWY = :line "
				") ");

		pointsQuery.bindValue(":text", textLayer);
		pointsQuery.bindValue(":line", lineLayer);

		if (pointsQuery.exec()) while (pointsQuery.next()) Base.append(
		{
			pointsQuery.value(0).toDouble(),
			pointsQuery.value(1).toDouble()
		});

		emit onSetupProgress(0, Base.size() * 2);

		QtConcurrent::blockingMap(Base, getList);

		for (auto P : Pools) if (!Rem.contains(P.first))
		{
			double X(0.0), Y(0.0); unsigned Count(0); Rem.insert(P.first);

			for (auto O : P.second) if (!Rem.contains(O))
			{
				X += O->x(); Y += O->y(); ++Count; Rem.insert(O);
			}

			if (Count)
			{
				X += P.first->x(); Y += P.first->y(); ++Count;

				Points.append({ X /= Count, Y /= Count });
			}

			emit onUpdateProgress(++Step);
		}
	}

	emit onBeginProgress(tr("Fitting geometry"));
	emit onSetupProgress(0, 0);

	const auto Updates = proceedGeometry(Points, Radius, textLayer, lineLayer, true);

	emit onBeginProgress(tr("Updating database"));
	emit onSetupProgress(0, Updates.size());

	QSqlQuery lineQuery(Database), roundQuery(Database), pointQuery(Database);

	lineQuery.prepare("UPDATE EW_POLYLINE SET P0_X = ?, P0_Y = ?, P1_X = ?, P1_Y = ? WHERE ID = ? AND P1_FLAGS IN (0, 4)");
	roundQuery.prepare("UPDATE EW_POLYLINE SET P0_X = ?, P0_Y = ?, PN_X = ?, PN_Y = ? WHERE ID = ? AND P1_FLAGS IN (2)");
	pointQuery.prepare("UPDATE EW_TEXT SET POS_X = ?, POS_Y = ? WHERE ID = ?");

	for (auto i = Updates.constBegin(); i != Updates.constEnd(); ++i) if (!isTerminated())
	{
		if (i.value().type() == QVariant::PointF)
		{
			const auto P = i.value().toPointF();

			pointQuery.addBindValue(P.x());
			pointQuery.addBindValue(P.y());
			pointQuery.addBindValue(i.key());

			pointQuery.exec();
		}
		else if (i.value().type() == QVariant::LineF)
		{
			const auto L = i.value().toLineF();

			lineQuery.addBindValue(L.x1());
			lineQuery.addBindValue(L.y1());
			lineQuery.addBindValue(L.x2());
			lineQuery.addBindValue(L.y2());
			lineQuery.addBindValue(i.key());

			lineQuery.exec();

			roundQuery.addBindValue(L.x1());
			roundQuery.addBindValue(L.y1());
			roundQuery.addBindValue(L.x2());
			roundQuery.addBindValue(L.y2());
			roundQuery.addBindValue(i.key());

			roundQuery.exec();
		}

		emit onUpdateProgress(++Step);
	}

	emit onEndProgress();
	emit onProceedEnd(Updates.size());
}

void DatabaseDriver::removeDuplicates(int Action, int Strategy, int Heurstic, int Type, int Layer, int Sublayer, double Radius)
{
	if (!Database.open()) { emit onProceedEnd(0); return; } int Count(0);

	if (Type == 0)
	{
		Count = proceedTextDuplicates(Action, Strategy, Heurstic, Layer, Sublayer, Radius);
	}
	else
	{
		Count = proceedLineDuplicates(Action, Strategy, Heurstic, Layer, Sublayer, Radius);
	}

	emit onEndProgress();
	emit onProceedEnd(Count);
}

void DatabaseDriver::hideDuplicates(const QSet<int>& Layers, bool Objected)
{
	if (!Database.open()) { emit onProceedEnd(0); return; } int Step(0);

	struct SEGMENT { int ID; QPointF A, B; };

	QList<SEGMENT> Lines; QSet<int> Hides, Limit; QMutex Synchronizer;

	emit onBeginProgress(tr("Loading lines"));
	emit onSetupProgress(0, 0);

	QSqlQuery selectQuery(Database), updateQuery(Database);

	if (Objected)
	{
		QSqlQuery objectedQuery(Database);

		objectedQuery.setForwardOnly(true);
		objectedQuery.prepare(
			"SELECT "
				"E.IDE "
			"FROM "
				"EW_OB_ELEMENTY E "
			"INNER JOIN "
				"EW_OBIEKTY O "
			"ON "
				"E.UIDO = O.UID "
			"WHERE "
				"O.STATUS = 0 AND"
				"E.TYP = 0");

		if (objectedQuery.exec()) while (objectedQuery.next())
		{
			Limit.insert(objectedQuery.value(0).toInt());
		}
	}

	selectQuery.prepare(
		"SELECT "
			"P.UID, P.ID, P.ID_WARSTWY, "
			"ROUND(P.P0_X, 2), "
			"ROUND(P.P0_Y, 2), "
			"ROUND(P.P1_X, 2), "
			"ROUND(P.P1_Y, 2) "
		"FROM "
			"EW_POLYLINE P "
		"WHERE "
			"P.STAN_ZMIANY = 0 AND "
			"P.P1_FLAGS = 0");

	if (selectQuery.exec()) while (selectQuery.next())
	{
		const bool OK = !Objected || Limit.contains(selectQuery.value(1).toInt());

		if (Layers.contains(selectQuery.value(2).toInt()))
		{
			if (OK) Lines.append(
			{
				selectQuery.value(0).toInt(),
				{
					selectQuery.value(3).toDouble(),
					selectQuery.value(4).toDouble()
				},
				{
					selectQuery.value(5).toDouble(),
					selectQuery.value(6).toDouble()
				}
			});
		}
	}

	selectQuery.prepare(
		"SELECT "
			"L.UID, L.ID_WARSTWY "
		"FROM "
			"EW_POLYLINE L "
		"INNER JOIN "
			"EW_OB_ELEMENTY E "
		"ON "
			"E.IDE = L.ID "
		"INNER JOIN "
			"EW_OBIEKTY O "
		"ON "
			"O.UID = E.UIDO "
		"WHERE "
			"L.STAN_ZMIANY = 0 AND "
			"E.TYP = 0 AND "
			"O.STATUS = 0 "
		"GROUP BY "
			"L.UID, L.ID_WARSTWY "
		"HAVING "
			"COUNT(O.UID) > 1");

	emit onBeginProgress(tr("Computing geometry"));
	emit onSetupProgress(0, 0);

	if (selectQuery.exec()) while (selectQuery.next())
	{
		if (Layers.contains(selectQuery.value(1).toInt()))
		{
			Hides.insert(selectQuery.value(0).toInt());
		}
	}

	QtConcurrent::blockingMap(Lines, [&Lines, &Hides, &Synchronizer] (SEGMENT& Segment) -> void
	{
		const auto compare = [] (const SEGMENT& A, const SEGMENT& B) -> bool
		{
			return (A.A == B.A && A.B == B.B) || (A.B == B.A && A.A == B.B);
		};

		for (const auto& Other : Lines) if (Other.ID != Segment.ID)
		{
			if (compare(Segment, Other))
			{
				Synchronizer.lock();
				Hides.insert(Segment.ID);
				Synchronizer.unlock();

				return;
			}
		}
	});

	emit onBeginProgress(tr("Updating lines"));
	emit onSetupProgress(0, Hides.size());

	updateQuery.prepare("UPDATE EW_POLYLINE SET TYP_LINII = 14 WHERE UID = ?");

	for (const auto& UID : Hides)
	{
		updateQuery.addBindValue(UID);
		updateQuery.exec();

		emit onUpdateProgress(++Step);
	}

	emit onEndProgress();
	emit onProceedEnd(Hides.size());
}

void DatabaseDriver::fitLabels(const QString& Class, int Source, int Dest, double Distance, double Spin, bool Info)
{
	if (!Database.open()) { emit onProceedEnd(0); return; } int Step(0);

	struct ITEM { int UID; int Type; QVariant Geometry; QSet<int> Appends; QString Info; };

	QSqlQuery selectPoint(Database), selectLine(Database), selectSymbols(Database), selectData(Database),
			appendLabel(Database), updateLayer(Database), updateData(Database);

	selectPoint.setForwardOnly(true);
	selectLine.setForwardOnly(true);
	selectSymbols.setForwardOnly(true);
	selectData.setForwardOnly(true);

	emit onBeginProgress("Loading geometry");
	emit onSetupProgress(0, 0);

	QSet<int> Unlabeled, Used; QHash<int, ITEM> Items; QMutex Locker;
	QHash<int, POINT> Texts = loadPoints(Source, false); int Inserts(0);

	const auto& Table = getItemByField(Tables, Class, &TABLE::Name);

	selectSymbols.prepare(
		"SELECT DISTINCT "
			"O.UID "
		"FROM "
			"EW_OBIEKTY O "
		"INNER JOIN "
			"EW_OB_ELEMENTY E "
		"ON "
			"O.UID = E.UIDO "
		"INNER JOIN "
			"EW_TEXT T "
		"ON "
			"E.IDE = T.ID "
		"WHERE "
			"O.STATUS = 0 AND "
			"T.STAN_ZMIANY = 0 AND "
			"E.TYP = 0 AND "
			"O.KOD = ? "
		"GROUP BY "
			"O.UID "
		"HAVING "
			"COUNT(NULLIF(T.TYP, 4)) = 0");

	selectPoint.prepare(
		"SELECT "
			"O.UID, P.POS_X, P.POS_Y "
		"FROM "
			"EW_OBIEKTY O "
		"INNER JOIN "
			"EW_OB_ELEMENTY E "
		"ON "
			"O.UID = E.UIDO "
		"INNER JOIN "
			"EW_TEXT P "
		"ON "
			"E.IDE = P.ID "
		"WHERE "
			"O.STATUS = 0 AND "
			"P.STAN_ZMIANY = 0 AND "
			"P.TYP = 4 AND "
			"E.TYP = 0 AND "
			"O.KOD = ?");

	selectLine.prepare(
		"SELECT "
			"O.UID, "
			"P.P1_FLAGS, P.P0_X, P.P0_Y, "
			"IIF(P.PN_X IS NULL, P.P1_X, P.PN_X), "
			"IIF(P.PN_Y IS NULL, P.P1_Y, P.PN_Y), "
			"O.RODZAJ "
		"FROM "
			"EW_OBIEKTY O "
		"INNER JOIN "
			"EW_OB_ELEMENTY E "
		"ON "
			"O.UID = E.UIDO "
		"INNER JOIN "
			"EW_POLYLINE P "
		"ON "
			"E.IDE = P.ID "
		"WHERE "
			"O.STATUS = 0 AND "
			"P.STAN_ZMIANY = 0 AND "
			"E.TYP = 0 AND "
			"O.KOD = ? "
		"ORDER BY "
			"E.UIDO ASCENDING,"
			"E.N ASCENDING");

	selectData.prepare(QString("SELECT UIDO, INFORMACJA FROM %1").arg(Table.Data));
	updateData.prepare(QString("UPDATE %1 SET INFORMACJA = ? WHERE UIDO = ?").arg(Table.Data));

	appendLabel.prepare("INSERT INTO EW_OB_ELEMENTY (UIDO, TYP, IDE, N) "
					"VALUES (?, 0, ?, "
						"(SELECT MAX(E.N) + 1 FROM EW_OB_ELEMENTY E WHERE E.UIDO = ?)"
					")");

	updateLayer.prepare("UPDATE EW_TEXT SET ID_WARSTWY = ? WHERE ID = ? AND STAN_ZMIANY = 0");

	selectSymbols.addBindValue(Class);
	selectPoint.addBindValue(Class);
	selectLine.addBindValue(Class);

	if (selectSymbols.exec()) while (selectSymbols.next() && !isTerminated())
	{
		Unlabeled.insert(selectSymbols.value(0).toInt());
	}

	if (selectPoint.exec()) while (selectPoint.next() && !isTerminated())
	{
		const int UID = selectPoint.value(0).toInt();

		if (Unlabeled.contains(UID))
		{
			Items.insert(UID,
			{
				UID, 4,
				QPointF(selectPoint.value(1).toDouble(),
					   selectPoint.value(2).toDouble())
			});

			emit onUpdateProgress(++Step);
		}
	}

	if (selectLine.exec()) while (selectLine.next() && !isTerminated())
	{
		const int UID = selectLine.value(0).toInt();

		const QLineF Line(selectLine.value(2).toDouble(),
					   selectLine.value(3).toDouble(),
					   selectLine.value(4).toDouble(),
					   selectLine.value(5).toDouble());

		if (!Items.contains(UID))
		{
			Items.insert(UID,
			{
				UID, selectLine.value(6).toInt(), QVariant()
			});
		}

		ITEM& Obj = Items[UID];

		if (selectLine.value(1).toInt() != 4)
		{
			QVariantList List = Obj.Geometry.isNull() ?
								QVariantList() :
								Obj.Geometry.toList();

			List.append(Line); Obj.Geometry = List;
		}
		else Obj.Geometry = Line;
	}

	if (Info && selectData.exec()) while (selectData.next() && !isTerminated())
	{
		const int UID = selectData.value(0).toInt();
		const QString Str = selectData.value(1).toString();

		if (Items.contains(UID)) Items[UID].Info = Str;
	}

	if (isTerminated()) { emit onProceedEnd(0); return; }

	const auto getPairs = [&Items, &Used, Distance, Spin] (POINT& P) -> void
	{
		static const auto length = [] (double x1, double y1, double x2, double y2)
		{
			const double dx = x1 - x2;
			const double dy = y1 - y2;

			return qSqrt(dx * dx + dy * dy);
		};

		if (Used.contains(P.ID)) return;

		for (const auto& L : Items)
		{
			if (L.Type == 3 && L.Geometry.toList().size() > 2)
			{
				const auto Geom = L.Geometry.toList();

				QPolygonF Pl; Pl.reserve(Geom.size() + 1);

				if (Geom.first().toLineF().p1() == Geom.last().toLineF().p1() ||
				    Geom.first().toLineF().p1() == Geom.last().toLineF().p2())
				{
					Pl.append(Geom.first().toLineF().p1());
				}
				else
				{
					Pl.append(Geom.first().toLineF().p2());
				}

				for (const auto G : L.Geometry.toList())
				{
					const auto Line = G.toLineF();

					if (!Pl.contains(Line.p1())) Pl.append(Line.p1());
					if (!Pl.contains(Line.p2())) Pl.append(Line.p2());
				}

				if (Pl.containsPoint({ P.X, P.Y }, Qt::OddEvenFill))
				{
					P.L = 0.0; P.Match = L.UID;
				}
			}

			if (L.Geometry.type() == QVariant::PointF)
			{
				const auto Point = L.Geometry.value<QPointF>();

				const double h = length(P.X, P.Y, Point.x(), Point.y());

				if (h <= Distance && (qIsNaN(P.L) || h < P.L))
				{
					P.L = h; P.Match = L.UID;
				}
			}
			else if (L.Geometry.type() == QVariant::LineF)
			{
				const auto Line = L.Geometry.value<QLineF>();

				const double X = (Line.x1() + Line.x2()) / 2.0;
				const double Y = (Line.y1() + Line.y2()) / 2.0;
				const double R = qAbs(Line.x1() - X);

				const double h = length(P.X, P.Y, X, Y) - R;

				if (h <= Distance && (qIsNaN(P.L) || h < P.L))
				{
					P.L = h; P.Match = L.UID;
				}
			}
			else for (const auto G : L.Geometry.toList())
			{
				const auto Line = G.toLineF();

				double dtg = P.FI + qAtan2(Line.x1() - Line.x2(), Line.y1() - Line.y2());

				while (dtg >= M_PI) dtg -= M_PI; while (dtg < 0.0) dtg += M_PI;

				if (!P.Pointer && (qAbs(dtg) > Spin) && (qAbs(dtg - M_PI) > Spin)) continue;

				const double a = length(P.X, P.Y, Line.x1(), Line.y1());
				const double b = length(P.X, P.Y, Line.x2(), Line.y2());

				if ((a * a <= Line.length() * Line.length() + b * b) &&
				    (b * b <= Line.length() * Line.length() + a * a))
				{
					const double A = P.X - Line.x1(); const double B = P.Y - Line.y1();
					const double C = Line.x2() - Line.x1(); const double D = Line.y2() - Line.y1();

					const double h = qAbs(A * D - C * B) / qSqrt(C * C + D * D);

					if (h <= Distance && (qIsNaN(P.L) || h < P.L))
					{
						P.L = h; P.Match = L.UID;
					}
				}
			}
		}
	};

	const auto getLabel = [&Texts, &Used, &Locker, Info] (ITEM& I) -> void
	{
		const bool isSingleton = I.Geometry.type() == QVariant::LineF ||
							I.Geometry.type() == QVariant::PointF;

		if (isSingleton && !I.Appends.isEmpty()) return;

		for (const auto& P : Texts) if (P.Match == I.UID && (!Info || I.Info.isEmpty() || P.Text == I.Info))
		{
			if (!I.Appends.contains(P.ID))
			{
				if (I.Info.isEmpty()) I.Info = P.Text;

				Locker.lock();
				Used.insert(P.ID);
				Locker.unlock();

				I.Appends.insert(P.ID);

				if (isSingleton) return;
			}
		}
	};

	const auto resetMatch = [] (POINT& P) -> void { P.Match = 0; P.L = NAN; };

	emit onBeginProgress("Fitting geometry");
	emit onSetupProgress(0, 0);

	bool Continue(true); do
	{
		const int Count = Used.size();

		QtConcurrent::blockingMap(Texts, resetMatch);
		QtConcurrent::blockingMap(Texts, getPairs);
		QtConcurrent::blockingMap(Items, getLabel);

		Continue = Count != Used.size();
	}
	while (Continue && !isTerminated());

	emit onBeginProgress("Updating database");
	emit onSetupProgress(0, Items.size());

	for (const auto& O : Items) if (!isTerminated())
	{
		for (const auto& ID : O.Appends)
		{
			appendLabel.addBindValue(O.UID);
			appendLabel.addBindValue(ID);
			appendLabel.addBindValue(O.UID);

			appendLabel.exec();

			updateLayer.addBindValue(Dest);
			updateLayer.addBindValue(ID);

			updateLayer.exec();
		}

		if (Info)
		{
			updateData.addBindValue(O.Info);
			updateData.addBindValue(O.UID);

			updateData.exec();
		}

		Inserts += O.Appends.size();

		emit onUpdateProgress(++Step);
	}

	emit onEndProgress();
	emit onProceedEnd(Inserts);
}

void DatabaseDriver::unifyJobs(void)
{
	if (!Database.open()) { emit onProceedEnd(0); return; } int Step(0);

	emit onBeginProgress("Loading jobs");
	emit onSetupProgress(0, 0);

	QSqlQuery Query(Database); Query.setForwardOnly(true);

	QHash<QString, QList<int>> Jobs; int Count(0);

	Query.prepare(
		"SELECT "
			"UID, NUMER "
		"FROM "
			"EW_OPERATY "
		"ORDER BY "
			"OPERACJA DESC");

	if (Query.exec()) while (Query.next())
	{
		Jobs[Query.value(1).toString()].append(Query.value(0).toInt());
	}

	emit onBeginProgress("Updating jobs");
	emit onSetupProgress(0, Jobs.size());

	for (auto& List : Jobs) if (List.size() > 1)
	{
		const int New = List.takeFirst(); QStringList Old;

		for (const auto& ID : List)
		{
			Old.append(QString::number(ID));
		}

		Query.exec(QString("UPDATE EW_OBIEKTY SET OPERAT = %1 WHERE OPERAT IN (%2)")
				 .arg(New).arg(Old.join(',')));
		Query.exec(QString("UPDATE EW_POLYLINE SET OPERAT = %1 WHERE OPERAT IN (%2)")
				 .arg(New).arg(Old.join(',')));
		Query.exec(QString("UPDATE EW_TEXT SET OPERAT = %1 WHERE OPERAT IN (%2)")
				 .arg(New).arg(Old.join(',')));

		Query.exec(QString("DELETE FROM EW_OPERATY WHERE UID IN (%1)").arg(Old.join(',')));
		Query.exec(QString("UPDATE EW_OPERATY SET "
					    "OSOZ = NULL, DTZ = NULL, OPERACJA = 1 "
					    "WHERE UID = %1)").arg(New));

		emit onUpdateProgress(++Step); Count += 1;
	}
	else emit onUpdateProgress(++Step);

	emit onEndProgress();
	emit onProceedEnd(Count);
}

void DatabaseDriver::reloadLayers(bool Hide)
{
	const bool OK = Hideempty != Hide && Database.isOpen();

	if (OK) emit onReload(loadLineLayers(Tables, Hide),
					  loadPointLayers(Tables, Hide),
					  loadTextLayers(Tables, Hide));

	Hideempty = Hide;
}

void DatabaseDriver::terminate(void)
{
	Terminator.lock();
	Terminated = true;
	Terminator.unlock();
}

bool operator == (const DatabaseDriver::FIELD& One, const DatabaseDriver::FIELD& Two)
{
	return
	(
		One.Type == Two.Type &&
		One.Name == Two.Name &&
		One.Label == Two.Label &&
		One.Dict == Two.Dict
	);
}

bool operator == (const DatabaseDriver::TABLE& One, const DatabaseDriver::TABLE& Two)
{
	return
	(
		One.Name == Two.Name &&
		One.Label == Two.Label &&
		One.Data == Two.Data &&
		One.Fields == Two.Fields
	);
}

QVariant getDataFromDict(QVariant Value, const QMap<QVariant, QString>& Dict, DatabaseDriver::TYPE Type)
{
	if (!Value.isValid()) return QVariant();

	if (Type == DatabaseDriver::BOOL && Dict.isEmpty())
	{
		return Value.toBool() ? DatabaseDriver::tr("Yes") : DatabaseDriver::tr("No");
	}

	if (Type == DatabaseDriver::MASK && !Dict.isEmpty())
	{
		QStringList Values; const int Bits = Value.toInt();

		for (auto i = Dict.constBegin(); i != Dict.constEnd(); ++i)
		{
			if (Bits & (1 << i.key().toInt())) Values.append(i.value());
		}

		return Values.join(", ");
	}

	if (Dict.isEmpty()) return Value;

	if (Dict.contains(Value)) return Dict[Value];
	else return DatabaseDriver::tr("Unknown");
}

template<class Type, class Field, template<class> class Container>
Type& getItemByField(Container<Type>& Items, const Field& Data, Field Type::*Pointer)
{
	for (auto& Item : Items) if (Item.*Pointer == Data) return Item;
}

template<class Type, class Field, template<class> class Container>
const Type& getItemByField(const Container<Type>& Items, const Field& Data, Field Type::*Pointer)
{
	for (auto& Item : Items) if (Item.*Pointer == Data) return Item;
}

template<class Type, class Field, template<class> class Container>
bool hasItemByField(const Container<Type>& Items, const Field& Data, Field Type::*Pointer)
{
	for (auto& Item : Items) if (Item.*Pointer == Data) return true; return false;
}
