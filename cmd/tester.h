/*
 * Skaffari - a mail account administration web interface based on Cutelyst
 * Copyright (C) 2017-2018 Matthias Fehring <mf@huessenbergnetz.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TESTER_H
#define TESTER_H

#include <QFileInfo>
#include <QCoreApplication>
#include "consoleoutput.h"

/*!
 * \ingroup skaffaricmd
 * \brief Handles checks for the Skaffari installation.
 */
class Tester : public ConsoleOutput
{
    Q_DECLARE_TR_FUNCTIONS(Tester)
public:
    /*!
     * \brief Constructs a new Tester object using the given \a confFile.
     * \param confFile  absolute path to the configuration file
     */
    explicit Tester(const QString &confFile);

    /*!
     * \brief Executes the test routines and returns \c 0 on success.
     */
    int exec() const;

private:
    QFileInfo m_confFile;
};

#endif // TESTER_H
