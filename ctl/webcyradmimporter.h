/*
 * Skaffari - a mail account administration web interface based on Cutelyst
 * Copyright (C) 2017 Matthias Fehring <kontakt@buschmann23.de>
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

#ifndef WEBCYRADMIMPORTER_H
#define WEBCYRADMIMPORTER_H

#include <QFileInfo>
#include "configinput.h"

class WebCyradmImporter : public ConfigInput
{
public:
    WebCyradmImporter(const QString &confFileName, const QString &iniFileName);

    int exec() const;

private:
    QFileInfo m_webCyradmConfFile;
    QFileInfo m_iniFile;

    QString getArrayEntry(const QString &array, const QString &key) const;
};

#endif // WEBCYRADMIMPORTER_H