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

QHash<QString, QHash<int, QString> > DatabaseDriver::loadLineLayers(const QList<DatabaseDriver::TABLE>& Tabs)
{
	if (!Database.isOpen()) return QHash<QString, QHash<int, QString>>();

	QSqlQuery Query(Database); Query.setForwardOnly(true);
	QHash<QString, QHash<int, QString>> lineLayers;

	for (const auto& Table : Tabs)
	{
		QHash<int, QString> L;

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

		lineLayers.insert(Table.Name, L);
	}

	return lineLayers;
}

QHash<QString, QHash<int, QString> > DatabaseDriver::loadPointLayers(const QList<TABLE>& Tabs)
{
	if (!Database.isOpen()) return QHash<QString, QHash<int, QString>>();

	QSqlQuery Query(Database); Query.setForwardOnly(true);
	QHash<QString, QHash<int, QString>> pointLayers;

	for (const auto& Table : Tabs)
	{
		QHash<int, QString> P;

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
					"T.NAZWA LIKE (O.KOD || '_%') "
				"ORDER BY "
					"G.NAZWA_L")
					    .arg(Table.Name));

			if (Query.exec()) while (Query.next())
			{
				P.insert(Query.value(0).toInt(), Query.value(1).toString());
			}
		}

		pointLayers.insert(Table.Name, P);
	}

	return pointLayers;
}

QHash<QString, QHash<int, QString>> DatabaseDriver::loadTextLayers(const QList<TABLE>& Tabs)
{
	if (!Database.isOpen()) return QHash<QString, QHash<int, QString>>();

	QSqlQuery Query(Database); Query.setForwardOnly(true);
	QHash<QString, QHash<int, QString>> textLayers;

	for (const auto& Table : Tabs)
	{
		QHash<int, QString> T;

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
					"T.NAZWA = O.KOD "
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

		textLayers.insert(Table.Name, T);
	}

	return textLayers;
}

QHash<int, DatabaseDriver::LINE> DatabaseDriver::loadLines(int Layer, int Flags)
{
	if (!Database.isOpen()) return QHash<int, LINE>(); QHash<int, LINE> Lines;

	QSqlQuery Query(Database); Query.setForwardOnly(true);

	Query.prepare(
		"SELECT "
			"P.ID, P.OPERAT, "
			"P.P0_X, P.P0_Y,"
			"P.P1_X, P.P1_Y, "
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
		0,
		Query.value(2).toDouble(),
		Query.value(3).toDouble(),
		Query.value(4).toDouble(),
		Query.value(5).toDouble(),
		Query.value(6).toInt()
	});

	QtConcurrent::blockingMap(Lines, [] (LINE& Line) -> void
	{
		const double dx = Line.X1 - Line.X2;
		const double dy = Line.Y1 - Line.Y2;

		Line.Len = qSqrt(dx * dx + dy * dy);
	});

	return Lines;
}

QHash<int, DatabaseDriver::POINT> DatabaseDriver::loadPoints(int Layer, int Type)
{
	if (!Database.isOpen()) return QHash<int, POINT>(); QHash<int, POINT> Points;

	QSqlQuery Query(Database); Query.setForwardOnly(true);

	Query.prepare(
		"SELECT "
			"P.ID, P.OPERAT, "
			"IIF(ODN_X IS NULL, P.POS_X, P.POS_X + ODN_X), "
			"IIF(ODN_Y IS NULL, P.POS_Y, P.POS_Y + ODN_Y), "
			"P.TEXT, P.TYP "
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

	if (Query.exec()) while (Query.next()) if (Type == -1 || Query.value(5).toInt() == Type) Points.insert(Query.value(0).toInt(),
	{
		Query.value(0).toInt(),
		Query.value(1).toInt(),
		0,
		Query.value(2).toDouble(),
		Query.value(3).toDouble(),
		NAN,
		Query.value(4).toString()
	});

	return Points;
}

QList<DatabaseDriver::OBJECT> DatabaseDriver::proceedLines(int Line, int Text)
{
	if (!Database.isOpen()) return QList<OBJECT>();

	QHash<int, LINE> Lines; QHash<int, POINT> Points;
	QList<QPointF> Cuts; QList<OBJECT> Objects;

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

	const auto getPairs = [&Lines] (POINT& P) -> void
	{
		static const auto length = [] (double x1, double y1, double x2, double y2)
		{
			const double dx = x1 - x2;
			const double dy = y1 - y2;

			return qSqrt(dx * dx + dy * dy);
		};

		for (const auto& L : Lines)
		{
			const double a = length(P.X, P.Y, L.X1, L.Y1);
			const double b = length(P.X, P.Y, L.X2, L.Y2);

			if ((a * a < L.Len * L.Len + b * b) &&
			    (b * b < a * a + L.Len * L.Len))
			{
				const double p = 0.5 * (L.Len + a + b);
				const double h = 2.0 * qSqrt(p * (p - a) * (p - b) * (p - L.Len)) / L.Len;

				if (qIsNaN(P.L) || h < P.L) { P.L = h; P.Match = L.ID; };
			}
		}
	};

	const auto getLabel = [&Points] (LINE& L) -> void
	{
		for (const auto& P : Points) if (P.Match == L.ID)
		{
			if (!L.IDK && P.IDK) L.IDK = P.IDK; L.Label = P.ID; return;
		}
	};

	const auto getObjects = [&Lines, &Points, &Cuts, &Objects] (void) -> void
	{
		const static auto append = [] (const LINE& L, OBJECT& O, const QHash<int, POINT>& P, QSet<int>& U) -> void
		{
			if (!O.IDK && L.IDK) O.IDK = L.IDK;

			if (L.Label)
			{
				if (!O.IDK && P[L.Label].IDK) O.IDK = P[L.Label].IDK;

				if (O.Label.isEmpty())
				{
					O.Label = P[L.Label].Text;
				}

				O.Geometry.append(L.Label);
				U.insert(L.Label);
			}

			O.Geometry.append(L.ID);
			U.insert(L.ID);
		};

		QSet<int> Used; for (const auto& S : Lines) if (!Used.contains(S.ID))
		{
			OBJECT O; bool Continue = true;

			QPointF P1(S.X1, S.Y1);
			QPointF P2(S.X2, S.Y2);

			append(S, O, Points, Used);

			while (Continue)
			{
				const int oldSize = O.Geometry.size();

				for (const auto& L : Lines) if (!Used.contains(L.ID))
				{
					const QString Label = L.Label ? Points[L.Label].Text : QString();

					if ((S.Style == L.Style) && (!L.IDK || S.IDK == L.IDK))
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
								case 1: P1 = L2; break;
								case 2: P1 = L1; break;
								case 3: P2 = L2; break;
								case 4: P2 = L1; break;
							}

							append(L, O, Points, Used);
						}
					}
				}

				Continue = oldSize != O.Geometry.size();
			}

			Objects.append(O);
		}
	};

	for (const auto& L : loadLines(Line, 0)) Lines.insert(L.ID, L);
	for (const auto& L : loadLines(Line, 2)) Lines.insert(L.ID, L);
	for (const auto& P : loadPoints(Text)) Points.insert(P.ID, P);

	if (Lines.isEmpty()) return QList<OBJECT>();

	QFutureSynchronizer<void> Synchronizer;

	Synchronizer.addFuture(QtConcurrent::run(getCuts));
	Synchronizer.addFuture(QtConcurrent::map(Points, getPairs));

	Synchronizer.waitForFinished();

	Synchronizer.addFuture(QtConcurrent::map(Lines, getLabel));

	Synchronizer.waitForFinished();

	Synchronizer.addFuture(QtConcurrent::run(getObjects));

	Synchronizer.waitForFinished();

	return Objects;
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
					loadLineLayers(Tables),
					loadPointLayers(Tables),
					loadTextLayers(Tables));

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

void DatabaseDriver::proceedClass(const QHash<int, QVariant>& Values, const QString& Pattern, const QString& Class, int Line, int Point, int Text)
{
	if (!Database.open()) { emit onProceedEnd(0); return; } QMap<int, QList<OBJECT>> List;

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
	};

	const auto& Table = getItemByField(Tables, Class, &TABLE::Name);

	List.insert(2, proceedLines(Line, Text));

	QFutureSynchronizer<void> Synchronizer; QStringList Labels;

	if ((Table.Flags & 103) == 103) {} // TODO proceed surfaces

	if ((Table.Flags & 109) == 109) Synchronizer.addFuture(QtConcurrent::map(List[2], getValues));

	if ((Table.Flags & 357) == 357) {} // TODO proceed points

	for (auto i = Values.constBegin(); i != Values.constEnd(); ++i)
	{
		Labels.append(Table.Fields[i.key()].Name); Labels.last().remove("EW_DATA.");
	}

	Synchronizer.waitForFinished();

	for (auto i = List.constBegin(); i != List.constEnd(); ++i) for (const auto O : i.value())
	{
		QSqlQuery objectQuery(Database), geometryQuery(Database); QStringList Set; int n = 0;

		for (const auto& V : O.Values) Set.append(V.toString().remove(QRegExp("\\$\\d+\\b")));

		objectQuery.exec("SELECT GEN_ID(EW_OBIEKTY_UID_GEN, 1) FROM RDB$DATABASE");

		QVariant ID = objectQuery.next() ? objectQuery.value(0) : QVariant();

		objectQuery.exec(QString(
			"INSERT INTO "
				"EW_OBIEKTY (UID, IDKATALOG, KOD, RODZAJ, OPERAT, OSOU, OSOW, DTU, DTW, STATUS) "
			"VALUES "
				"(%1, 1, '%2', %3, %4, 0, 0, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP, 0)")
					  .arg(ID.toInt())
					  .arg(Table.Name)
					  .arg(i.key())
					  .arg(O.IDK));

		objectQuery.exec(QString(
			"INSERT INTO "
				"%1 (UIDO, %2) "
			"VALUES "
				"('%3', '%4')")
					  .arg(Table.Data)
					  .arg(Labels.join(", "))
					  .arg(ID.toInt())
					  .arg(Set.join("', '")));

		geometryQuery.prepare(
			"INSERT INTO "
				"EW_OB_ELEMENTY (UIDO, N, TYP, IDE) "
			"VALUES"
				"(:id, :n, 0, :ide)");

		geometryQuery.bindValue(":id", ID);

		for (const auto& IDE : O.Geometry)
		{
			geometryQuery.bindValue(":n", n++);
			geometryQuery.bindValue(":ide", IDE);

			geometryQuery.exec();
		}
	}

	emit onProceedEnd(List[2].size());
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
