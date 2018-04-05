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

#ifndef SKAFFARI_H
#define SKAFFARI_H

#include <Cutelyst/Application>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(SK_CORE)

using namespace Cutelyst;

/*!
 * \defgroup skaffaricore SkaffariCore
 * \brief %Skaffari core application
 */

/*!
 * \ingroup skaffaricore
 * \brief Main application class.
 */
class Skaffari : public Application
{
    Q_OBJECT
    CUTELYST_APPLICATION(IID "Skaffari")
public:
    /*!
     * \brief Constructs a new application instance.
     */
    Q_INVOKABLE explicit Skaffari(QObject *parent = nullptr);

    /*!
     * \brief Destroys the application instance.
     */
    ~Skaffari();

    /*!
     * \brief Initializes the application and returns \c true on success.
     */
    bool init() override;

    /*!
     * \brief This will be called after the engine forked and will setup the database connection.
     *
     * Returns always \c true.
     */
    bool postFork() override;

private:
    bool initDb() const;
    static bool isInitialized;
    static bool messageHandlerInstalled;
};

#endif //SKAFFARI_H

