/****************************************************************************/
/// @file    TL_LQF_MWM.h
/// @author  Mani Amoozadeh <maniam@ucdavis.edu>
/// @date    Jul 2015
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

#ifndef TRAFFICLIGHTLQFMWM_H
#define TRAFFICLIGHTLQFMWM_H

#include <09_TL_LowDelay.h>

namespace VENTOS {

class TrafficLight_LQF_MWM : public TrafficLightLowDelay
{
  public:
    virtual ~TrafficLight_LQF_MWM();
    virtual void finish();
    virtual void initialize(int);
    virtual void handleMessage(cMessage *);

  protected:
    void virtual executeFirstTimeStep();
    void virtual executeEachTimeStep(bool);

  private:
    void chooseNextInterval();
    void chooseNextGreenInterval();

  protected:
    bool greenExtension;

    enum vehTypeWeight
    {
        weight_emergency = 50,
        weight_pedestrian = 40,
        weight_bicycle = 30,
        weight_passenger = 20,
        weight_bus = 10,
        weight_truck = 1,
    };
};

}

#endif
