/****************************************************************************/
/// @file    LoopDetectors.h
/// @author  Mani Amoozadeh <maniam@ucdavis.edu>
/// @author
/// @date    April 2015
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

#ifndef LOOPDETECTORS_H
#define LOOPDETECTORS_H

#include <01_TL_Base.h>
#include <boost/circular_buffer.hpp>

namespace VENTOS {

class LoopDetectorData
{
  public:
    std::string detectorName;
    std::string lane;
    std::string vehicleName;
    double entryTime;
    double leaveTime;
    double entrySpeed;
    double leaveSpeed;

    LoopDetectorData( std::string str1, std::string str2, std::string str3, double entryT=-1, double leaveT=-1, double entryS=-1, double leaveS=-1 )
    {
        this->detectorName = str1;
        this->lane = str2;
        this->vehicleName = str3;
        this->entryTime = entryT;
        this->leaveTime = leaveT;
        this->entrySpeed = entryS;
        this->leaveSpeed = leaveS;
    }

    friend bool operator== (const LoopDetectorData &v1, const LoopDetectorData &v2)
    {
        return ( v1.detectorName == v2.detectorName && v1.vehicleName == v2.vehicleName );
    }
};


class currentQueueSize
{
public:
    double time;
    std::string TLid;
    int totoalQueueSize;
    int laneCount;

    currentQueueSize(double d1, std::string str1, int i1, int i2)
    {
        this->time = d1;
        this->TLid = str1;
        this->totoalQueueSize = i1;
        this->laneCount = i2;
    }
};


class currentStatusTL
{
public:
    std::string allowedMovements;
    double greenStart;
    double yellowStart;
    double redStart;
    double phaseEnd;
    int incommingLanes;
    int totalQueueSize;

    currentStatusTL(std::string str1, double d1, double d2, double d3, double d4, int i1, int i2)
    {
        this->allowedMovements = str1;
        this->greenStart = d1;
        this->yellowStart = d2;
        this->redStart = d3;
        this->phaseEnd = d4;
        this->incommingLanes = i1;
        this->totalQueueSize = i2;
    }
};


class LoopDetectors : public TrafficLightBase
{
  public:
    virtual ~LoopDetectors();
    virtual void finish();
    virtual void initialize(int);
    virtual void handleMessage(cMessage *);

  protected:
    void virtual executeFirstTimeStep();
    void virtual executeEachTimeStep(bool);

  private:
    void getAllDetectors();

    void collectLDsData();
    void saveLDsData();

    void measureTrafficParameters();
    void saveTLQueueingData();
    void saveTLPhasingData();

  protected:
    // NED variables
    bool measureTrafficDemand;
    bool measureIntersectionQueue;

    bool collectInductionLoopData;
    bool collectTLQueuingData;
    bool collectTLPhasingData;

    std::list<std::string> TLList;   // list of traffic-lights in the network

    std::map<std::string /*lane*/, std::pair<std::string /*LD id*/, double /*last actuation*/>> LD_demand;   // ids of loop detectors used for measuring incoming traffic demand
    std::map<std::string /*lane*/, std::string /*LD id*/> LD_actuated;                                       // ids of loop detectors used for actuated-time signal control
    std::map<std::string /*lane*/, std::string /*AD id*/> AD_queue;                                          // ids of area detectors used for measuring queue length

    std::map<std::string /*lane*/, std::string /*TLid*/> lanesTL;                                         // all incoming lanes for each intersection
    std::multimap<std::string /*lane*/, std::pair<std::string /*TLid*/, int /*link number*/>> linksTL;    // all outgoing link # for each incoming lane

    std::map<std::string /*lane*/, std::pair<std::string /*TLid*/,int /*queue size*/>> laneQueueSize;   // real-time queue size for each incoming lane for each intersection
    std::map<std::pair<std::string /*TLid*/,int /*link*/>, int /*queue size*/> linkQueueSize;           // real-time queue size for each link in each intersection
    std::map<std::string /*TLid*/, std::pair<int /*total queue*/, int /*lane count*/>> totalQueueSize;  // real-time total queue size of all incoming lanes for each TLid

    std::map<std::string /*lane*/, std::pair<std::string /*TLid*/, boost::circular_buffer<double> /*TD*/>> laneTD;   // real-time traffic demand for each incoming lane for each intersection
    std::map<std::pair<std::string /*TLid*/,int /*link*/>, boost::circular_buffer<double> /*TD*/> linkTD;            // real-time traffic demand for each link in each intersection

    std::map<std::string /*TLid*/, int /*phase number*/> phaseTL;                                  // current phase in each TL
    std::map<std::pair<std::string /*TLid*/, int /*phase number*/>, currentStatusTL> statusTL;     // current status of each TL in each phase

  private:
    std::vector<LoopDetectorData> Vec_loopDetectors;
    std::vector<currentQueueSize> Vec_totalQueueSize;
};

}

#endif
