/****************************************************************************/
/// @file    ApplPed_02_Beacon.cc
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

#include "ApplPed_02_Beacon.h"

namespace VENTOS {

Define_Module(VENTOS::ApplPedBeacon);

ApplPedBeacon::~ApplPedBeacon()
{

}


void ApplPedBeacon::initialize(int stage)
{
    ApplPedBase::initialize(stage);

    if (stage == 0)
    {
        // NED
        VANETenabled = par("VANETenabled").boolValue();
        GPSerror = par("GPSerror").doubleValue();

        // NED variables (beaconing parameters)
        sendBeacons = par("sendBeacons").boolValue();
        beaconInterval = par("beaconInterval").doubleValue();
        maxOffset = par("maxOffset").doubleValue();
        beaconLengthBits = par("beaconLengthBits").longValue();
        beaconPriority = par("beaconPriority").longValue();

        // NED variables
        smartBeaconing = par("smartBeaconing").boolValue();

        // NED variables (data parameters)
        dataLengthBits = par("dataLengthBits").longValue();
        dataOnSch = par("dataOnSch").boolValue();
        dataPriority = par("dataPriority").longValue();

        // simulate asynchronous channel access
        double offSet = dblrand() * (beaconInterval/2);
        offSet = offSet + floor(offSet/0.050)*0.050;
        individualOffset = dblrand() * maxOffset;

        PedestrianBeaconEvt = new cMessage("BeaconEvt", KIND_TIMER);
        if (VANETenabled)
            scheduleAt(simTime() + offSet, PedestrianBeaconEvt);

        crossing = false;
        leaving = false;
    }
}


void ApplPedBeacon::finish()
{
    ApplPedBase::finish();

    if (PedestrianBeaconEvt->isScheduled())
    {
        cancelAndDelete(PedestrianBeaconEvt);
    }
    else
    {
        delete PedestrianBeaconEvt;
    }
}


void ApplPedBeacon::handleSelfMsg(cMessage* msg)
{
    ApplPedBase::handleSelfMsg(msg);

    if (msg == PedestrianBeaconEvt)
    {
        if(VANETenabled)
        {
            if(smartBeaconing)
                smartBeaconingDecision();

            if(sendBeacons)
            {
                BeaconPedestrian* beaconMsg = ApplPedBeacon::prepareBeacon();

                // send it
                sendDelayed(beaconMsg, individualOffset, lowerLayerOut);
            }
        }

        // schedule for next beacon broadcast
        scheduleAt(simTime() + beaconInterval, PedestrianBeaconEvt);
    }
}


void ApplPedBeacon::smartBeaconingDecision()
{
    Coord myPos = TraCI->vehicleGetPosition(SUMOID);

    // pedestrian enters the zone
    // todo: change from fixed coordinates
    // coordinates should be a little bigger than the detection region
    // the pedestrian should start beaconing a little bit sooner
    if( (myPos.x >= 830) && (myPos.x <= 960) && (myPos.y >= 830) && (myPos.y <= 960) )
    {
        // get the current edge
        std::string myEdge = TraCI->vehicleGetEdgeID(SUMOID);

        // started to cross
        if( !crossing && (myEdge[0] == ':') && (myEdge[1] == 'C') )
        {
            crossing = true;
            sendBeacons = true;   // keep beaconing 'on' during crossing
        }
        // crossed the intersection
        else if( crossing && ((myEdge[0] != ':') || (myEdge[1] != 'C')) )
        {
            crossing = false;
            leaving = true;
            sendBeacons = false;
        }
        else if(leaving)
        {
            sendBeacons = false;
        }
        // not crossed yet or during crossing
        else
        {
            sendBeacons = true;
        }
    }
    else
        sendBeacons = false;
}


BeaconPedestrian*  ApplPedBeacon::prepareBeacon()
{
    BeaconPedestrian* wsm = new BeaconPedestrian("beaconPedestrian");

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
    wsm->setSender(SUMOID.c_str());
    wsm->setSenderType(SUMOType.c_str());
    wsm->setRecipient("broadcast");

    // set current position
    Coord cord = TraCI->personGetPosition(SUMOID);
    wsm->setPos(cord);

    // set current speed
    wsm->setSpeed( TraCI->personGetSpeed(SUMOID) );

    // set current acceleration
    wsm->setAccel( -1 );

    // set maxDecel
    wsm->setMaxDecel( -1 );

    // set current lane
    wsm->setLane( TraCI->personGetEdgeID(SUMOID).c_str() );

    return wsm;
}


// is called, every time the position of pedestrian changes
void ApplPedBeacon::handlePositionUpdate(cObject* obj)
{
    ApplPedBase::handlePositionUpdate(obj);
}

}

