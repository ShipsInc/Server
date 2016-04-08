/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef _WIN32
  #include <winsock2.h>
#endif
#include <mysql.h>
#include <mysqld_error.h>
#include <errmsg.h>
#include <thread>

#include "MySQLConnection.h"

MySQLConnection::MySQLConnection() : m_Mysql(nullptr), m_connectionInfo(MySQLConnectionInfo()) { }

MySQLConnection::~MySQLConnection()
{
    if (m_Mysql)
        mysql_close(m_Mysql);
}

bool MySQLConnection::Open(MySQLConnectionInfo connectInfo)
{
    m_connectionInfo = connectInfo;

    MYSQL* mysqlInit;
    mysqlInit = mysql_init(NULL);
    if (!mysqlInit)
    {
        std::cout << "Could not initialize Mysql connection to database " << m_connectionInfo.database.c_str();
        return false;
    }

    int port;
    char const* unix_socket;
    //unsigned int timeout = 10;

    mysql_options(mysqlInit, MYSQL_SET_CHARSET_NAME, "utf8");
    //mysql_options(mysqlInit, MYSQL_OPT_READ_TIMEOUT, (char const*)&timeout);
    #ifdef _WIN32
    if (m_connectionInfo.host == ".")                                           // named pipe use option (Windows)
    {
        unsigned int opt = MYSQL_PROTOCOL_PIPE;
        mysql_options(mysqlInit, MYSQL_OPT_PROTOCOL, (char const*)&opt);
        port = 0;
        unix_socket = 0;
    }
    else                                                    // generic case
    {
        port = atoi(m_connectionInfo.port_or_socket.c_str());
        unix_socket = 0;
    }
    #else
    if (m_connectionInfo.host == ".")                                           // socket use option (Unix/Linux)
    {
        unsigned int opt = MYSQL_PROTOCOL_SOCKET;
        mysql_options(mysqlInit, MYSQL_OPT_PROTOCOL, (char const*)&opt);
        m_connectionInfo.host = "localhost";
        port = 0;
        unix_socket = m_connectionInfo.port_or_socket.c_str();
    }
    else                                                    // generic case
    {
        port = atoi(m_connectionInfo.port_or_socket.c_str());
        unix_socket = nullptr;
    }
    #endif

    m_Mysql = mysql_real_connect(mysqlInit, m_connectionInfo.host.c_str(), m_connectionInfo.user.c_str(),
        m_connectionInfo.password.c_str(), m_connectionInfo.database.c_str(), port, unix_socket, 0);

    if (m_Mysql)
    {
        if (!m_reconnecting)
        {
            std::cout << "MySQL client library: " <<  mysql_get_client_info() << std::endl;
            std::cout << "MySQL server ver: " << mysql_get_server_info(m_Mysql) << std::endl;
        }

        std::cout << "Connected to MySQL database at " << m_connectionInfo.host.c_str() << std::endl;
        mysql_autocommit(m_Mysql, 1);

        // set connection properties to UTF8 to properly handle locales for different
        // server configs - core sends data in UTF8, so MySQL must expect UTF8 too
        mysql_set_character_set(m_Mysql, "utf8");
    }
    else
    {
        std::cout << "Could not connect to MySQL database at " << m_connectionInfo.host.c_str() << ". " << mysql_error(mysqlInit) << std::endl;
        mysql_close(mysqlInit);
        return false;
    }

    return true;
}

void MySQLConnection::Close()
{
    delete this;
}

bool MySQLConnection::Execute(const char* sql)
{
    if (!m_Mysql)
        return false;

    {
        if (mysql_query(m_Mysql, sql))
        {
            uint32 lErrno = mysql_errno(m_Mysql);
            if (_HandleMySQLErrno(lErrno))  // If it returns true, an error was handled successfully (i.e. reconnection)
                return Execute(sql);       // Try again

            return false;
        }
    }

    return true;
}

QueryResult MySQLConnection::Query(const char* sql)
{
    if (!sql)
        return NULL;

    MYSQL_RES *result = NULL;
    MYSQL_FIELD *fields = NULL;
    uint64 rowCount = 0;
    uint32 fieldCount = 0;

    if (!_Query(sql, &result, &fields, &rowCount, &fieldCount))
        return NULL;

    return std::make_shared<ResultSet>(result, fields, rowCount, fieldCount);
}

bool MySQLConnection::_Query(const char *sql, MYSQL_RES **pResult, MYSQL_FIELD **pFields, uint64* pRowCount, uint32* pFieldCount)
{
    if (!m_Mysql)
        return false;

    {
        if (mysql_query(m_Mysql, sql))
        {
            uint32 lErrno = mysql_errno(m_Mysql);
            if (_HandleMySQLErrno(lErrno))      // If it returns true, an error was handled successfully (i.e. reconnection)
                return _Query(sql, pResult, pFields, pRowCount, pFieldCount);    // We try again

            return false;
        }

        *pResult = mysql_store_result(m_Mysql);
        *pRowCount = mysql_affected_rows(m_Mysql);
        *pFieldCount = mysql_field_count(m_Mysql);
    }

    if (!*pResult )
        return false;

    if (!*pRowCount)
    {
        mysql_free_result(*pResult);
        return false;
    }

    *pFields = mysql_fetch_fields(*pResult);

    return true;
}

bool MySQLConnection::_HandleMySQLErrno(uint32 errNo)
{
    switch (errNo)
    {
        case CR_SERVER_GONE_ERROR:
        case CR_SERVER_LOST:
        case CR_INVALID_CONN_HANDLE:
        case CR_SERVER_LOST_EXTENDED:
        {
            m_reconnecting = true;
            mysql_close(GetHandle());
            if (this->Open(m_connectionInfo))                           // Don't remove 'this' pointer unless you want to skip loading all prepared statements....
            {
                std::cout << "Connection to the MySQL server is active." << std::endl;
                m_reconnecting = false;
                return true;
            }

            uint32 lErrno = mysql_errno(GetHandle());   // It's possible this attempted reconnect throws 2006 at us. To prevent crazy recursive calls, sleep here.
            std::this_thread::sleep_for(std::chrono::seconds(3)); // Sleep 3 seconds
            return _HandleMySQLErrno(lErrno);                     // Call self (recursive)
        }
        case ER_LOCK_DEADLOCK:
            return false;    // Implemented in TransactionTask::Execute and DatabaseWorkerPool<T>::DirectCommitTransaction
        // Query related errors - skip query
        case ER_WRONG_VALUE_COUNT:
        case ER_DUP_ENTRY:
            return false;

        // Outdated table or database structure - terminate core
        case ER_BAD_FIELD_ERROR:
        case ER_NO_SUCH_TABLE:
            std::cout << "Your database structure is not up to date. Please make sure you've executed all queries in the sql/updates folders." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10));
            std::abort();
            return false;
        case ER_PARSE_ERROR:
            std::cout << "Error while parsing SQL. Core fix required." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10));
            std::abort();
            return false;
        default:
            std::cout << "Unhandled MySQL errno " << errNo << " Unexpected behaviour possible." << std::endl;
            return false;
    }
}
