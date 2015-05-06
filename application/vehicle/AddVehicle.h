/****************************************************************************/
/// @file    AddVehicle.h
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

#ifndef VEHICLEADD
#define VEHICLEADD

#include "BaseModule.h"
#include "TraCI_Extend.h"

#define SSTR( x ) dynamic_cast< std::ostringstream & >( (std::ostringstream() << std::dec << x ) ).str()

namespace VENTOS {

class AddVehicle : public BaseModule
{
	public:
		virtual ~AddVehicle();
		virtual void initialize(int stage);
        virtual void handleMessage(cMessage *msg);
		virtual void finish();
	    virtual void receiveSignal(cComponent *, simsignal_t, long);

	private:

        // NED variables
        cModule *nodePtr;   // pointer to the Node
        TraCI_Extend *TraCI;  // pointer to the TraCI module
        double terminate;
        simsignal_t Signal_executeFirstTS;
        bool on;
        int mode;
	    int totalVehicles;
	    double lambda;
	    int plnSize;
	    double plnSpace;

	    // methods
        void Add();

	    void Scenario1();
        void Scenario2();
        void Scenario3();
        void Scenario4();
        void Scenario5();
        void Scenario6();
        void Scenario7();
        void Scenario8();
        void Scenario9();
};

}

#endif
