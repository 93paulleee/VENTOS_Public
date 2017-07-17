/****************************************************************************/
/// @file    BaseWaveApplLayer.h
/// @author  Mani Amoozadeh <maniam@ucdavis.edu>
/// @author  second author name
/// @date    Feb 2017
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
// This program is distributed in the hope that it will be useful}},
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#ifndef BASEWAVEAPPLLAYER_H_
#define BASEWAVEAPPLLAYER_H_

#include "MIXIM_veins/annotations/AnnotationManager.h"
#include "MIXIM_veins/nic/mac/Mac1609_4.h"
#include "baseAppl/03_BaseApplLayer.h"
#include "traci/TraCICommands.h"
#include "global/Statistics.h"
#include "logging/VENTOS_logging.h"

namespace VENTOS {

class BaseWaveApplLayer : public BaseApplLayer
{
private:
    typedef BaseApplLayer super;

protected:
    enum WaveApplMessageTypes {
        TYPE_BEACON_VEHICLE,     // beaconVehicle
        TYPE_BEACON_BICYCLE,     // beaconBicycle
        TYPE_BEACON_OBSTACLE,
        TYPE_BEACON_PEDESTRIAN,  // beaconPedestrian
        TYPE_BEACON_RSU,         // beaconRSU

        TYPE_LANECHANGE_DATA,    // laneChange
        TYPE_PLATOON_DATA,       // platoonMsg
        TYPE_CRL_PIECE,
        TYPE_SAEJ2735_BSM,
        TYPE_BROADCAST_DATA,
        TYPE_PAYMENT_REQUEST,
        TYPE_PAYMENT_RESPONSE,
    };

    static const simsignalwrap_t mobilityStateChangedSignal;
    static const simsignalwrap_t parkingStateChangedSignal;

    TraCI_Commands* TraCI = NULL;
    BaseMobility* mobility = NULL;
    Statistics* STAT = NULL;
    Veins::Mac1609_4* mac = NULL;

    /* BSM (beacon) settings */
    bool sendBeacons;
    uint32_t beaconLengthBits;
    uint32_t beaconPriority;
    omnetpp::simtime_t beaconInterval;

    /* WSM (data) settings */
    uint32_t dataLengthBits;
    uint32_t dataPriority;
    bool dataOnSCH;

    bool DSRCenabled;
    bool hasOBU;
    std::string IPaddress;

    /* info in OMNET++ */
    int myId;
    const char *myFullId;
    Coord curPosition;  // OMNET++ position
    Coord curSpeed;

    /* info in SUMO */
    std::string SUMOID;
    std::string SUMOType;
    std::string vehicleClass;

    /* messages for periodic beacon broadcast */
    omnetpp::cMessage* sendBeaconEvt = NULL;

public:
    ~BaseWaveApplLayer();
    virtual void initialize(int stage);
    virtual void finish();

    virtual void receiveSignal(omnetpp::cComponent* source, omnetpp::simsignal_t signalID, omnetpp::cObject* obj, omnetpp::cObject* details);

protected:
    /** @brief handle messages from below */
    virtual void handleLowerMsg(omnetpp::cMessage* msg);

    /** @brief handle self messages */
    virtual void handleSelfMsg(omnetpp::cMessage* msg);

    /** @brief this method is called every beaconInterval */
    virtual void sendBeacon();

    /** @brief this function is called every time the vehicle receives a position update signal */
    virtual void handlePositionUpdate(omnetpp::cObject* obj);

    /** @brief this function is called every time the vehicle parks or starts moving again */
    virtual void handleParkingUpdate(omnetpp::cObject* obj);

    /** @brief compute a point in time that is guaranteed to be in the correct channel interval plus a random offset */
    virtual omnetpp::simtime_t computeAsynchronousSendingTime(omnetpp::simtime_t interval, Veins::t_channel chantype);
};

}

#endif
