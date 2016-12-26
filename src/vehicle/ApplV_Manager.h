/****************************************************************************/
/// @file    ApplV_Manager.h
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

#ifndef ApplVMANAGER_H
#define ApplVMANAGER_H

#include "ApplV_05_MsgControl.h"

namespace VENTOS {

class ApplVManager : public ApplVMsgControl
{
private:
    typedef ApplVMsgControl super;

protected:
    // NED variables (packet loss ratio)
    double droppT;
    std::string droppV;
    double plr;

    // NED variables (measurement error)
    bool measurementError;
    double errorGap;
    double errorRelSpeed;

    bool reportBeaconsData;

    long BeaconVehCount;
    long BeaconVehDropped;
    long BeaconRSUCount;
    long PlatoonCount;
    long BeaconPedCount;

public:
    ~ApplVManager();
    virtual void initialize(int stage);
    virtual void finish();
    virtual void receiveSignal(omnetpp::cComponent* source, omnetpp::simsignal_t signalID, omnetpp::cObject* obj, cObject* details);

protected:
    virtual void handleLowerMsg(omnetpp::cMessage*);
    virtual void handleSelfMsg(omnetpp::cMessage*);
    virtual void handlePositionUpdate(omnetpp::cObject*);

    virtual void onBeaconVehicle(BeaconVehicle*);
    virtual void onBeaconPedestrian(BeaconPedestrian*);
    virtual void onBeaconRSU(BeaconRSU*);
    virtual void onPlatoonMsg(PlatoonMsg*);

    bool dropBeacon(double time, std::string vehicle, double plr);
};

}

#endif
