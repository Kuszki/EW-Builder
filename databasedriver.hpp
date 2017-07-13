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

#ifndef DATABASEDRIVER_HPP
#define DATABASEDRIVER_HPP

#include <QSharedDataPointer>
#include <QSqlDatabase>
#include <QSqlRecord>
#include <QSqlQuery>
#include <QSqlError>
#include <QPolygonF>
#include <QSettings>
#include <QFileInfo>
#include <QVariant>
#include <QObject>
#include <QHash>

#include <QtConcurrent>

#include <QDebug>

class DatabaseDriver : public QObject
{

		Q_OBJECT

	struct POINT
	{
		int ID, IDK, Match;

		double X, Y, L, FI;

		QString Text;
	};

	struct LINE
	{
		int ID, IDK, Label;

		QSet<int> Labels;

		double X1, Y1;
		double X2, Y2;

		int Style;

		double Rad = NAN;
		double Len = NAN;
		double Odn = NAN;
	};

	struct OBJECT
	{
		QHash<int, QVariant> Values;
		QList<int> Geometry;
		QList<int> Labels;

		QString Label;
		int IDK = 0;
	};

	public: enum TYPE
	{
		READONLY	= 0,
		STRING	= 1,
		INTEGER	= 4,
		SMALLINT	= 5,
		BOOL		= 7,
		DOUBLE	= 8,
		DATE		= 101,
		MASK		= 102,
		DATETIME	= 1000
	};

	public: struct FIELD
	{
		TYPE Type;

		QString Name;
		QString Label;

		QMap<QVariant, QString> Dict;
	};

	public: struct TABLE
	{
		QString Name;
		QString Label;
		QString Data;

		int Flags;

		QList<FIELD> Fields;
		QList<int> Indexes;
		QList<int> Headers;
	};

	private:

		mutable QMutex Terminator;

		QSqlDatabase Database;
		QStringList Headers;

		QList<TABLE> Tables;
		QList<FIELD> Fields;
		QList<FIELD> Common;

		bool Terminated = false;

	public:

		static const QStringList Operators;

		explicit DatabaseDriver(QObject* Parent = nullptr);
		virtual ~DatabaseDriver(void) override;

	protected:

		QList<FIELD> loadCommon(bool Emit = false);
		QList<TABLE> loadTables(bool Emit = false);

		QList<FIELD> loadFields(const QString& Table) const;
		QMap<QVariant, QString> loadDict(const QString& Field, const QString& Table) const;

		QList<FIELD> normalizeFields(QList<TABLE>& Tabs, const QList<FIELD>& Base) const;
		QStringList normalizeHeaders(QList<TABLE>& Tabs, const QList<FIELD>& Base) const;

		QHash<QString, QHash<int, QString>> loadLineLayers(const QList<TABLE>& Tabs);
		QHash<QString, QHash<int, QString>> loadPointLayers(const QList<TABLE>& Tabs);
		QHash<QString, QHash<int, QString>> loadTextLayers(const QList<TABLE>& Tabs);

		QHash<int, LINE> loadLines(int Layer, int Flags = 0);
		QHash<int, POINT> loadPoints(int Layer, int Type = -1);

		QList<OBJECT> proceedLines(int Line, int Text,
							  const QString& Expr = QString(),
							  double Length = qInf(),
							  bool Job = false,
							  bool Keep = false);

		QList<OBJECT> proceedPoints(int Symbol, int Text,
							   const QString& Expr = QString(),
							   double Length = qInf(),
							   bool Job = false);

		QList<OBJECT> proceedSurfaces(int Line, int Text,
								const QString& Expr = QString(),
								double Length = qInf(),
								bool Job = false);

		QList<OBJECT> proceedTexts(int Text, const QString& Expr = QString());

		bool isTerminated(void) const;

	public slots:

		bool openDatabase(const QString& Server, const QString& Base,
					   const QString& User, const QString& Pass);

		bool closeDatabase(void);

		void proceedClass(const QHash<int, QVariant>& Values,
					   const QString& Pattern, const QString& Class,
					   int Line, int Point, int Text,
					   double Length, bool Keep, bool Job,
					   const QString& Insert);

		void proceedJobs(const QString& Path, const QString& Sep,
					  int xPos, int yPos, int jobPos);

		void terminate(void);

	signals:

		void onError(const QString&);

		void onConnect(const QList<TABLE>&, unsigned,
					const QHash<QString, QHash<int, QString>>&,
					const QHash<QString, QHash<int, QString>>&,
					const QHash<QString, QHash<int, QString>>&);
		void onDisconnect(void);
		void onLogin(bool);

		void onBeginProgress(const QString&);
		void onSetupProgress(int, int);
		void onUpdateProgress(int);
		void onEndProgress(void);

		void onProceedEnd(int);

};

bool operator == (const DatabaseDriver::FIELD& One, const DatabaseDriver::FIELD& Two);
bool operator == (const DatabaseDriver::TABLE& One, const DatabaseDriver::TABLE& Two);

QVariant getDataFromDict(QVariant Value, const QMap<QVariant, QString>& Dict, DatabaseDriver::TYPE Type);

template<class Type, class Field, template<class> class Container>
Type& getItemByField(Container<Type>& Items, const Field& Data, Field Type::*Pointer);

template<class Type, class Field, template<class> class Container>
const Type& getItemByField(const Container<Type>& Items, const Field& Data, Field Type::*Pointer);

template<class Type, class Field, template<class> class Container>
bool hasItemByField(const Container<Type>& Items, const Field& Data, Field Type::*Pointer);

#endif // DATABASEDRIVER_HPP
