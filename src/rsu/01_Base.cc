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

#include "rsu/01_Base.h"

namespace VENTOS {

Define_Module(VENTOS::ApplRSUBase);

ApplRSUBase::~ApplRSUBase()
{

}


void ApplRSUBase::initialize(int stage)
{
    super::initialize(stage);

    if (stage==0)
    {
        // get a pointer to the TraCI module
        cModule *module = omnetpp::getSimulation()->getSystemModule()->getSubmodule("TraCI");
        TraCI = static_cast<TraCI_Commands *>(module);
        ASSERT(TraCI);

        // get a pointer to the TrafficLight module
        TLptr = omnetpp::getSimulation()->getSystemModule()->getSubmodule("TrafficLight");
        ASSERT(TLptr);
        TLControlMode = TLptr->par("TLControlMode").longValue();
        minGreenTime = TLptr->par("minGreenTime").doubleValue();

        headerLength = par("headerLength").longValue();

        // NED variables (beaconing parameters)
        sendBeacons = par("sendBeacons").boolValue();
        beaconInterval = par("beaconInterval").doubleValue();
        maxOffset = par("maxOffset").doubleValue();
        beaconLengthBits = par("beaconLengthBits").longValue();
        beaconPriority = par("beaconPriority").longValue();

        // id in omnet++
        myId = getParentModule()->getIndex();
        myFullId = getParentModule()->getFullName();

        // id in SUMO
        SUMOID = par("SUMOID").stringValue();

        myTLid = par("myTLid").stringValue();   // TLid that this RSU belongs to (this parameter is set by AddRSU)
                                                // empty string means this RSU is not associated with any TL

        // my X coordinate in SUMO
        myCoordX = this->getParentModule()->getSubmodule("mobility")->par("x").doubleValue();
        // my Y coordinate in SUMO
        myCoordY = this->getParentModule()->getSubmodule("mobility")->par("y").doubleValue();

        // simulate asynchronous channel access
        double offSet = dblrand() * (beaconInterval/2);
        offSet = offSet + floor(offSet/0.050)*0.050;
        individualOffset = dblrand() * maxOffset;

        if (sendBeacons)
        {
            RSUBeaconEvt = new omnetpp::cMessage("RSUBeaconEvt", TYPE_TIMER);
            scheduleAt(omnetpp::simTime() + offSet, RSUBeaconEvt);
        }
    }
}


void ApplRSUBase::finish()
{
    super::finish();

    // unsubscribe
    omnetpp::getSimulation()->getSystemModule()->unsubscribe("executeEachTS", this);
}


void ApplRSUBase::handleSelfMsg(omnetpp::cMessage* msg)
{
    if (msg == RSUBeaconEvt)
    {
        BeaconRSU* beaconMsg = generateBeacon();

        EV << "## Created beacon msg for " << myFullId << std::endl;

        // send it
        sendDelayed(beaconMsg, individualOffset, lowerLayerOut);

        // schedule for next beacon broadcast
        scheduleAt(omnetpp::simTime() + beaconInterval, RSUBeaconEvt);
    }
    else
        throw omnetpp::cRuntimeError("Can't handle msg %s of kind %d", msg->getFullName(), msg->getKind());
}


void ApplRSUBase::executeEachTimeStep()
{

}


BeaconRSU* ApplRSUBase::generateBeacon()
{
    BeaconRSU* wsm = new BeaconRSU("beaconRSU", TYPE_BEACON_RSU);

    // add header length
    wsm->addBitLength(headerLength);

    // add payload length
    wsm->addBitLength(beaconLengthBits);

    wsm->setWsmVersion(1);
    wsm->setSecurityType(1);

    wsm->setChannelNumber(Veins::Channels::CCH);

    wsm->setDataRate(1);
    wsm->setPriority(beaconPriority);
    wsm->setPsid(0);

    // wsm->setSerial(serial);
    // wsm->setTimestamp(simTime());

    // fill in the sender/receiver fields
    wsm->setSender(myFullId);
    wsm->setSenderType("notSpecified");
    wsm->setRecipient("broadcast");

    // set my current position
    Coord SUMOpos (myCoordX, myCoordY);
    wsm->setPos(SUMOpos);

    return wsm;
}


}

