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

class DatabaseDriver : public QObject
{

		Q_OBJECT

	struct POINT
	{
		int ID, IDK, Match;

		double X, Y, L, FI;

		QString Text;

		bool Pointer;
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
		bool Hideempty = false;

	public:

		static const QStringList Operators;

		explicit DatabaseDriver(QObject* Parent = nullptr);
		virtual ~DatabaseDriver(void) override;

		bool isConnected(void) const;

	protected:

		QList<FIELD> loadCommon(bool Emit = false);
		QList<TABLE> loadTables(bool Emit = false);

		QList<FIELD> loadFields(const QString& Table) const;
		QMap<QVariant, QString> loadDict(const QString& Field, const QString& Table) const;

		QList<FIELD> normalizeFields(QList<TABLE>& Tabs, const QList<FIELD>& Base) const;
		QStringList normalizeHeaders(QList<TABLE>& Tabs, const QList<FIELD>& Base) const;

		QHash<QString, QHash<int, QString>> loadLineLayers(const QList<TABLE>& Tabs, bool Hide);
		QHash<QString, QHash<int, QString>> loadPointLayers(const QList<TABLE>& Tabs, bool Hide);
		QHash<QString, QHash<int, QString>> loadTextLayers(const QList<TABLE>& Tabs, bool Hide);

		QHash<int, LINE> loadLines(int Layer, int Flags = 0);
		QHash<int, POINT> loadPoints(int Layer, bool Symbol = true);

		QList<OBJECT> proceedLines(int Line, int Text,
							  const QString& Expr = QString(),
							  double Length = qInf(),
							  double Spin = 0.15,
							  int Job = false,
							  bool Keep = false);

		QList<OBJECT> proceedPoints(int Symbol, int Text,
							   const QString& Expr = QString(),
							   double Length = qInf(),
							   int Job = false);

		QList<OBJECT> proceedSurfaces(int Line, int Text,
								const QString& Expr = QString(),
								double Length = qInf(),
								int Job = false);

		QList<OBJECT> proceedTexts(int Text, int Point, const QString& Expr = QString(), int Fit = 0,
							  double Length = qInf(), const QString& Symbol = QString());

		QHash<int, QVariant> proceedGeometry(const QList<QPointF>& Points, double Radius, bool Emit = false);

		bool isTerminated(void) const;

	public slots:

		bool openDatabase(const QString& Server, const QString& Base,
					   const QString& User, const QString& Pass);

		bool closeDatabase(void);

		void proceedClass(const QHash<int, QVariant>& Values,
					   const QString& Pattern, const QString& Class,
					   int Line, int Point, int Text, double Length,
					   double Spin, bool Keep, int Job, int Snap,
					   const QString& Insert);

		void proceedJobs(const QString& Path, const QString& Sep,
					  int xPos, int yPos, int jobPos);

		void proceedFit(const QString& Path, int xPos, int yPos, double Radius);

		void reloadLayers(bool Hide);

		void terminate(void);

	signals:

		void onError(const QString&);

		void onConnect(const QList<TABLE>&, unsigned,
					const QHash<QString, QHash<int, QString>>&,
					const QHash<QString, QHash<int, QString>>&,
					const QHash<QString, QHash<int, QString>>&);
		void onDisconnect(void);
		void onLogin(bool);

		void onReload(const QHash<QString, QHash<int, QString>>&,
				    const QHash<QString, QHash<int, QString>>&,
				    const QHash<QString, QHash<int, QString>>&);

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
