/****************************************************************************/
/// @file    TrafficLightBase.h
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

#ifndef TRAFFICLIGHTBASE_H
#define TRAFFICLIGHTBASE_H

#include <BaseModule.h>
#include <Appl.h>
#include "TraCI_Extend.h"

using namespace std;

namespace VENTOS {

class TLVehicleData
{
  public:
    int index;
    double time;
    char vehicleName[20];
    char lane[20];
    double pos;
    double speed;
    char TLid[20];
    int yellowOrRedSignal;

    TLVehicleData(int i, double d1,
                const char *str1, const char *str2,
                double d2, double d3,
                const char *str3, int s)
    {
        this->index = i;
        this->time = d1;

        strcpy(this->vehicleName, str1);
        strcpy(this->lane, str2);

        this->pos = d2;
        this->speed = d3;

        strcpy(this->TLid, str3);
        yellowOrRedSignal = s;
    }
};


class TrafficLightBase : public BaseModule
{
  public:
      virtual ~TrafficLightBase();
      virtual void finish();
      virtual void initialize(int);
      virtual void handleMessage(cMessage *);
      virtual void receiveSignal(cComponent *, simsignal_t, long);

  protected:
      double updateInterval;
      bool collectTLData;
      int TLControlMode;

      TraCI_Extend *TraCI;
      simsignal_t Signal_executeFirstTS;
      simsignal_t Signal_executeEachTS;
      list<string> TLList;   // list of traffic-lights in the network
      vector<TLVehicleData *> Vec_vehiclesData;
      int index;

  protected:
      virtual void executeFirstTimeStep();
      virtual void executeEachTimeStep(bool);

  private:
      void TLData();
      void saveTLData(string);
      string getTrafficLightController(string vID);
      void TLDataToFile();
};

}

#endif
