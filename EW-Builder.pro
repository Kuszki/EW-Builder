#* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
#*                                                                         *
#*  Generate objects from layers for EWMAPA software                       *
#*  Copyright (C) 2016  Łukasz "Kuszki" Dróżdż  l.drozdz@o2.pl             *
#*                                                                         *
#*  This program is free software: you can redistribute it and/or modify   *
#*  it under the terms of the GNU General Public License as published by   *
#*  the  Free Software Foundation, either  version 3 of the  License, or   *
#*  (at your option) any later version.                                    *
#*                                                                         *
#*  This  program  is  distributed  in the hope  that it will be useful,   *
#*  but WITHOUT ANY  WARRANTY;  without  even  the  implied  warranty of   *
#*  MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the   *
#*  GNU General Public License for more details.                           *
#*                                                                         *
#*  You should have  received a copy  of the  GNU General Public License   *
#*  along with this program. If not, see http://www.gnu.org/licenses/.     *
#*                                                                         *
#* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *

QT			+=	core gui widgets concurrent sql network

TARGET		=	EW-Builder
TEMPLATE		=	app

CONFIG		+=	c++19


SOURCES		+=	main.cpp \
				mainwindow.cpp \
				aboutdialog.cpp \
				connectdialog.cpp \
				databasedriver.cpp \
				updatewidget.cpp \
				proceeddialog.cpp \
				jobsdialog.cpp \
				fitdialog.cpp \
				duplicatesdialog.cpp \
				hidedialog.cpp \
				labeldialog.cpp

HEADERS		+=	mainwindow.hpp \
				aboutdialog.hpp \
				connectdialog.hpp \
				databasedriver.hpp \
				updatewidget.hpp \
				proceeddialog.hpp \
				jobsdialog.hpp \
				fitdialog.hpp \
				duplicatesdialog.hpp \
				hidedialog.hpp \
				labeldialog.hpp

FORMS		+=	mainwindow.ui \
				aboutdialog.ui \
				connectdialog.ui \
				updatewidget.ui \
				proceeddialog.ui \
				jobsdialog.ui \
				fitdialog.ui \
				duplicatesdialog.ui \
				hidedialog.ui \
				labeldialog.ui

RESOURCES		+= 	resources.qrc

TRANSLATIONS	+=	ew-builder_pl.ts

QMAKE_CXXFLAGS	+=	-std=c++14
