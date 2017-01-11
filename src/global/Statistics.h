/****************************************************************************/
/// @file    Statistics.h
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

#ifndef STATISTICS_H
#define STATISTICS_H

#include "MIXIM/modules/BaseApplLayer.h"
#include "MIXIM/modules/ChannelAccess.h"
#include "veins/WaveAppToMac1609_4Interface.h"
#include "traci/TraCICommands.h"

namespace VENTOS {


class plnManagement
{
public:
    double time;
    std::string sender;
    std::string receiver;
    std::string type;
    std::string sendingPlnID;
    std::string receivingPlnID;

    plnManagement(double t, std::string str1, std::string str2, std::string str3, std::string str4, std::string str5)
    {
        this->time = t;
        this->sender = str1;
        this->receiver = str2;
        this->type = str3;
        this->sendingPlnID = str4;
        this->receivingPlnID = str5;
    }
};


class plnStat
{
public:
    double time;
    std::string from;
    std::string to;
    std::string maneuver;

    plnStat(double t, std::string str1, std::string str2, std::string str3)
    {
        this->time = t;
        this->from = str1;
        this->to = str2;
        this->maneuver = str3;
    }
};


class BeaconStat
{
public:
    double time;
    std::string senderID;
    std::string receiverID;
    bool dropped;

    BeaconStat(double t, std::string str1, std::string str2, bool b)
    {
        this->time = t;
        this->senderID = str1;
        this->receiverID = str2;
        this->dropped = b;
    }
};


class Statistics : public BaseApplLayer
{
private:
    // NED variables
    bool reportPlnManagerData;
    bool reportBeaconsData;

    // NED variables
    TraCI_Commands *TraCI;

    // class variables (signals)
    omnetpp::simsignal_t Signal_initialize_withTraCI;
    omnetpp::simsignal_t Signal_executeEachTS;

    omnetpp::simsignal_t Signal_SentPlatoonMsg;
    omnetpp::simsignal_t Signal_VehicleState;
    omnetpp::simsignal_t Signal_PlnManeuver;
    omnetpp::simsignal_t Signal_beacon;

    // class variables (vectors)
    std::vector<plnManagement> Vec_plnManagement;
    std::vector<plnStat> Vec_plnStat;
    std::vector<BeaconStat> Vec_Beacons;

public:
    virtual ~Statistics();
    virtual void finish();
    virtual void initialize(int);
    virtual void handleMessage(omnetpp::cMessage *);
    virtual void receiveSignal(omnetpp::cComponent *, omnetpp::simsignal_t, long, cObject* details);
    virtual void receiveSignal(omnetpp::cComponent *, omnetpp::simsignal_t, cObject *, cObject* details);

private:
    void initialize_withTraCI();
    void executeEachTimestep();

    void plnManageToFile();
    void plnStatToFile();

    void beaconToFile();

    int getNodeIndex(std::string);
};

}

#endif
