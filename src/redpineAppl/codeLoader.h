/****************************************************************************/
/// @file    codeLoader.h
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

#ifndef CODELOADER_H_
#define CODELOADER_H_

#include <BaseApplLayer.h>
#include "TraCICommands.h"
#include "SSH_Helper.h"

// un-defining ev!
// why? http://stackoverflow.com/questions/24103469/cant-include-the-boost-filesystem-header
#undef ev
#include "boost/filesystem.hpp"
#define ev  (*cSimulation::getActiveEnvir())

namespace VENTOS {

class codeLoader : public BaseApplLayer
{
public:
    virtual ~codeLoader();
    virtual void initialize(int stage);
    virtual void finish();
    virtual void handleMessage(cMessage *msg);
    virtual void receiveSignal(cComponent *, simsignal_t, long);

private:
    void make_connection();
    void init_board(cModule *, SSH_Helper *);
    void init_test(cModule *, SSH_Helper *);

    void initialize_withTraCI();
    void executeEachTimestep();

private:
    typedef BaseApplLayer super;

    // NED variables
    TraCI_Commands *TraCI;  // pointer to the TraCI module
    bool on;

    std::string initScriptName = "";
    boost::filesystem::path remoteDir_Driver = "";
    boost::filesystem::path remoteDir_SourceCode = "";
    boost::filesystem::path redpineAppl_FullPath;

    simsignal_t Signal_executeEachTS;
    simsignal_t Signal_initialize_withTraCI;

    // active SSH sessions
    std::vector< std::pair<cModule *, SSH_Helper *> > active_SSH;
    std::mutex lock_vector;
};

}

#endif
