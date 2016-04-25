/****************************************************************************/
/// @file    SSH.h
/// @author  Mani Amoozadeh <maniam@ucdavis.edu>
/// @author  second author name
/// @date    Apr 2016
///
/****************************************************************************/
// VENTOS, Vehicular Network Open Simulator; see http:?
// Copyright (C) 2013-2015
/****************************************************************************/
//
// This file is part of VENTOS.
// VENTOS is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#ifndef SSHCONNECT_H_
#define SSHCONNECT_H_

#include <string>

#include <libssh/libsshpp.hpp>
#include <libssh/sftp.h>
#include <fcntl.h>   // def of  O_WRONLY | O_CREAT | O_TRUNC
#include <stdlib.h>
#include <mutex>

// un-defining ev!
// why? http://stackoverflow.com/questions/24103469/cant-include-the-boost-filesystem-header
#undef ev
#include "boost/filesystem.hpp"
#define ev  (*cSimulation::getActiveEnvir())

namespace VENTOS {

class SSH
{
public:
    SSH(std::string, int, std::string, std::string, bool = true);
    virtual ~SSH();

    void copyFile_SFTP(boost::filesystem::path, boost::filesystem::path);
    void copyFileStr_SFTP(std::string, std::string, boost::filesystem::path);
    std::vector<sftp_attributes> listDir(boost::filesystem::path dirpath);
    void syncDir(boost::filesystem::path, boost::filesystem::path);

    ssh_channel openShell();
    void closeShell(ssh_channel);
    double rebootDev(ssh_channel, int);
    void run_command(ssh_channel, std::string, int, bool);

    std::string getHostName();
    std::string getHostAddress();
    int getPort();
    std::string getUsername();
    std::string getPassword();

private:
    void checkHost(std::string host, bool);
    void authenticate(std::string password);
    int authenticate_kbdint();
    int verify_knownhost();
    void createSession_SFTP();
    bool last_command_failed(ssh_channel);

private:
    typedef std::chrono::high_resolution_clock::time_point Htime_t;

    static std::mutex lock_prompt;
    static std::mutex lock_print;

    std::string dev_hostName = "";
    std::string dev_hostIP = "";
    int dev_port = -1;
    std::string dev_username = "";
    std::string dev_password = "";

    ssh_session SSH_session;
    sftp_session SFTP_session;
};

}

#endif
