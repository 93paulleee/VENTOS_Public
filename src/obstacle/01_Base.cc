/****************************************************************************/
/// @file    Base.cc
/// @author  Mani Amoozadeh <maniam@ucdavis.edu>
/// @author  second author name
/// @date    August 2013
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

#include "obstacle/01_Base.h"

namespace VENTOS {

Define_Module(VENTOS::ApplObstacleBase);

ApplObstacleBase::~ApplObstacleBase()
{

}

void ApplObstacleBase::initialize(int stage)
{
	super::initialize(stage);

	if (stage==0)
	{
        // get a pointer to the TraCI module
        cModule *module = omnetpp::getSimulation()->getSystemModule()->getSubmodule("TraCI");
        TraCI = static_cast<TraCI_Commands *>(module);
        ASSERT(TraCI);

        headerLength = par("headerLength").longValue();

        // bike id in omnet++
		myId = getParentModule()->getIndex();
        // bike full id in omnet++
		myFullId = getParentModule()->getFullName();
        // bike id in sumo
        SUMOID = getParentModule()->par("SUMOID").stdstringValue();
        // bike type in sumo
        SUMOType = getParentModule()->par("SUMOType").stdstringValue();
        // vehicle class in sumo
        vehicleClass = getParentModule()->par("vehicleClass").stdstringValue();

        hasOBU = getParentModule()->par("hasOBU").boolValue();
        IPaddress = getParentModule()->par("IPaddress").stringValue();

        // store the time of entry
        entryTime = omnetpp::simTime().dbl();
	}
}


void ApplObstacleBase::finish()
{

}


void ApplObstacleBase::handleSelfMsg(omnetpp::cMessage* msg)
{
    throw omnetpp::cRuntimeError("Can't handle msg %s of kind %d", msg->getFullName(), msg->getKind());
}


}

