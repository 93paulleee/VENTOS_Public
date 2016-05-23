//
// Copyright (C) 2006-2012 Christoph Sommer <christoph.sommer@uibk.ac.at>
//
// Documentation for these modules is at http://veins.car2x.org/
//
// This program is free software; you can redistribute it and/or modify
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

#include <limits>
#include <iostream>
#include <sstream>

#include "TraCIMobility.h"

namespace {
    const double MY_INFINITY = (std::numeric_limits<double>::has_infinity ? std::numeric_limits<double>::infinity() : std::numeric_limits<double>::max());
}

namespace VENTOS {

Define_Module(VENTOS::TraCIMobilityMod);

const simsignalwrap_t TraCIMobilityMod::parkingStateChangedSignal = simsignalwrap_t(TRACI_SIGNAL_PARKING_CHANGE_NAME);

void TraCIMobilityMod::initialize(int stage)
{
	if (stage == 0)
	{
		BaseMobility::initialize(stage);

        // get a pointer to the TraCI module
        cModule *module =  omnetpp::getSimulation()->getSystemModule()->getSubmodule("TraCI");
        TraCI = static_cast<TraCI_Commands *>(module);
        ASSERT(TraCI);

        // get the ptr of the current module
        cModule *applm = this->getParentModule()->getSubmodule("appl");
        ASSERT(applm);
        vClassEnum = applm->par("vehicleClassEnum");

		debug = par("debug");
		antennaPositionOffset = par("antennaPositionOffset");
		accidentCount = par("accidentCount");

		ASSERT(isPreInitialized);
		isPreInitialized = false;

		Coord nextPos = calculateAntennaPosition(roadPosition);
		nextPos.z = move.getCurrentPosition().z;

		move.setStart(nextPos);
		move.setDirectionByVector(Coord(cos(angle), -sin(angle)));
		move.setSpeed(speed);

		isParking = false;
		startAccidentMsg = 0;
		stopAccidentMsg = 0;
		last_speed = -1;

		if (accidentCount > 0)
		{
		    omnetpp::simtime_t accidentStart = par("accidentStart");
			startAccidentMsg = new omnetpp::cMessage("scheduledAccident");
			stopAccidentMsg = new omnetpp::cMessage("scheduledAccidentResolved");
			scheduleAt(omnetpp::simTime() + accidentStart, startAccidentMsg);
		}
	}
	else if (stage == 1)
	{
		// don't call BaseMobility::initialize(stage) -- our parent will take care to call changePosition later
	}
	else
	{
		BaseMobility::initialize(stage);
	}
}

void TraCIMobilityMod::finish()
{
	cancelAndDelete(startAccidentMsg);
	cancelAndDelete(stopAccidentMsg);

	isPreInitialized = false;
}

void TraCIMobilityMod::handleSelfMsg(omnetpp::cMessage *msg)
{
	if (msg == startAccidentMsg)
	{
	    TraCI->vehicleSetSpeed(getExternalId(), 0);
	    omnetpp::simtime_t accidentDuration = par("accidentDuration");
		scheduleAt(omnetpp::simTime() + accidentDuration, stopAccidentMsg);
		accidentCount--;
	}
	else if (msg == stopAccidentMsg)
	{
        TraCI->vehicleSetSpeed(getExternalId(), -1);
		if (accidentCount > 0)
		{
		    omnetpp::simtime_t accidentInterval = par("accidentInterval");
			scheduleAt(omnetpp::simTime() + accidentInterval, startAccidentMsg);
		}
	}
}

void TraCIMobilityMod::preInitialize(std::string external_id, const Coord& position, std::string road_id, double speed, double angle)
{
	this->external_id = external_id;
	this->lastUpdate = 0;
	this->roadPosition = position;
	this->road_id = road_id;
	this->speed = speed;
	this->angle = angle;
	this->antennaPositionOffset = par("antennaPositionOffset");

	Coord nextPos = calculateAntennaPosition(roadPosition);
	nextPos.z = move.getCurrentPosition().z;

	move.setStart(nextPos);
	move.setDirectionByVector(Coord(cos(angle), -sin(angle)));
	move.setSpeed(speed);

	isPreInitialized = true;
}

void TraCIMobilityMod::nextPosition(const Coord& position, std::string road_id, double speed, double angle, VehicleSignal vehSignals)
{
	if (debug) EV << "nextPosition " << position.x << " " << position.y << " " << road_id << " " << speed << " " << angle << std::endl;
	isPreInitialized = false;
	this->roadPosition = position;
	this->road_id = road_id;
	this->speed = speed;
	this->angle = angle;
	this->vehSignals = vehSignals;

	changePosition();
}

void TraCIMobilityMod::changePosition()
{
	// ensure we're not called twice in one time step
	ASSERT(lastUpdate != omnetpp::simTime());

	Coord nextPos = calculateAntennaPosition(roadPosition);
	nextPos.z = move.getCurrentPosition().z;

	this->lastUpdate = omnetpp::simTime();

	move.setStart(Coord(nextPos.x, nextPos.y, move.getCurrentPosition().z)); // keep z position
	move.setDirectionByVector(Coord(cos(angle), -sin(angle)));
	move.setSpeed(speed);

	if (omnetpp::cSimulation::getActiveEnvir()->isGUI())
	    updateDisplayString();

	fixIfHostGetsOutside();
	updatePosition();
}


void TraCIMobilityMod::changeParkingState(bool newState)
{
	isParking = newState;
	emit(parkingStateChangedSignal, this);
}


void TraCIMobilityMod::updateDisplayString()
{
    ASSERT(-M_PI <= angle);
    ASSERT(angle < M_PI);

    // if this is a motor vehicle (we visualize all types of motor vehicles the same)
    if(vClassEnum == SVC_PASSENGER || vClassEnum == SVC_PRIVATE || vClassEnum == SVC_EMERGENCY || vClassEnum == SVC_BUS || vClassEnum == SVC_TRUCK)
    {
        getParentModule()->getDisplayString().setTagArg("b", 2, "rect");
        getParentModule()->getDisplayString().setTagArg("b", 3, "red");
        getParentModule()->getDisplayString().setTagArg("b", 4, "black");
        getParentModule()->getDisplayString().setTagArg("b", 5, "1");
    }
    // if this is a bicycle
    else if(vClassEnum == SVC_BICYCLE)
    {
        getParentModule()->getDisplayString().setTagArg("b", 2, "rect");
        getParentModule()->getDisplayString().setTagArg("b", 3, "yellow");
        getParentModule()->getDisplayString().setTagArg("b", 4, "black");
        getParentModule()->getDisplayString().setTagArg("b", 5, "1");
    }
    // if this is a pedestrian
    else if(vClassEnum == SVC_PEDESTRIAN)
    {
        getParentModule()->getDisplayString().setTagArg("b", 2, "rect");
        getParentModule()->getDisplayString().setTagArg("b", 3, "green");
        getParentModule()->getDisplayString().setTagArg("b", 4, "black");
        getParentModule()->getDisplayString().setTagArg("b", 5, "1");
    }
    else
        throw omnetpp::cRuntimeError("Unknown vClass type '%d'", vClassEnum);

    if (angle < -M_PI + 0.5 * M_PI_4 * 1)
    {
        getParentModule()->getDisplayString().setTagArg("t", 0, "\u2190");
        getParentModule()->getDisplayString().setTagArg("b", 0, "4");
        getParentModule()->getDisplayString().setTagArg("b", 1, "2");
    }
    else if (angle < -M_PI + 0.5 * M_PI_4 * 3)
    {
        getParentModule()->getDisplayString().setTagArg("t", 0, "\u2199");
        getParentModule()->getDisplayString().setTagArg("b", 0, "3");
        getParentModule()->getDisplayString().setTagArg("b", 1, "3");
    }
    else if (angle < -M_PI + 0.5 * M_PI_4 * 5)
    {
        getParentModule()->getDisplayString().setTagArg("t", 0, "\u2193");
        getParentModule()->getDisplayString().setTagArg("b", 0, "2");
        getParentModule()->getDisplayString().setTagArg("b", 1, "4");
    }
    else if (angle < -M_PI + 0.5 * M_PI_4 * 7)
    {
        getParentModule()->getDisplayString().setTagArg("t", 0, "\u2198");
        getParentModule()->getDisplayString().setTagArg("b", 0, "3");
        getParentModule()->getDisplayString().setTagArg("b", 1, "3");
    }
    else if (angle < -M_PI + 0.5 * M_PI_4 * 9)
    {
        getParentModule()->getDisplayString().setTagArg("t", 0, "\u2192");
        getParentModule()->getDisplayString().setTagArg("b", 0, "4");
        getParentModule()->getDisplayString().setTagArg("b", 1, "2");
    }
    else if (angle < -M_PI + 0.5 * M_PI_4 * 11)
    {
        getParentModule()->getDisplayString().setTagArg("t", 0, "\u2197");
        getParentModule()->getDisplayString().setTagArg("b", 0, "3");
        getParentModule()->getDisplayString().setTagArg("b", 1, "3");
    }
    else if (angle < -M_PI + 0.5 * M_PI_4 * 13)
    {
        getParentModule()->getDisplayString().setTagArg("t", 0, "\u2191");
        getParentModule()->getDisplayString().setTagArg("b", 0, "2");
        getParentModule()->getDisplayString().setTagArg("b", 1, "4");
    }
    else if (angle < -M_PI + 0.5 * M_PI_4 * 15)
    {
        getParentModule()->getDisplayString().setTagArg("t", 0, "\u2196");
        getParentModule()->getDisplayString().setTagArg("b", 0, "3");
        getParentModule()->getDisplayString().setTagArg("b", 1, "3");
    }
    else
    {
        getParentModule()->getDisplayString().setTagArg("t", 0, "\u2190");
        getParentModule()->getDisplayString().setTagArg("b", 0, "4");
        getParentModule()->getDisplayString().setTagArg("b", 1, "2");
    }
}


void TraCIMobilityMod::fixIfHostGetsOutside()
{
	Coord pos = move.getStartPos();
	Coord dummy = Coord::ZERO;
	double dum;

	bool outsideX = (pos.x < 0) || (pos.x >= playgroundSizeX());
	bool outsideY = (pos.y < 0) || (pos.y >= playgroundSizeY());
	bool outsideZ = (!world->use2D()) && ((pos.z < 0) || (pos.z >= playgroundSizeZ()));
	if (outsideX || outsideY || outsideZ)
	    throw omnetpp::cRuntimeError("Tried moving host to (%f, %f) which is outside the playground", pos.x, pos.y);

	handleIfOutside(RAISEERROR, pos, dummy, dummy, dum);
}


Coord TraCIMobilityMod::calculateAntennaPosition(const Coord& vehiclePos) const
{
	Coord corPos;

	if (antennaPositionOffset >= 0.001)
		//calculate antenna position of vehicle according to antenna offset
		corPos = Coord(vehiclePos.x - antennaPositionOffset*cos(angle), vehiclePos.y + antennaPositionOffset*sin(angle), vehiclePos.z);
	else
		corPos = Coord(vehiclePos.x, vehiclePos.y, vehiclePos.z);

	return corPos;
}


double TraCIMobilityMod::calculateCO2emission(double v, double a) const
{
    // Calculate CO2 emission parameters according to:
    // Cappiello, A. and Chabini, I. and Nam, E.K. and Lue, A. and Abou Zeid, M., "A statistical model of vehicle emissions and fuel consumption," IEEE 5th International Conference on Intelligent Transportation Systems (IEEE ITSC), pp. 801-809, 2002

    double A = 1000 * 0.1326; // W/m/s
    double B = 1000 * 2.7384e-03; // W/(m/s)^2
    double C = 1000 * 1.0843e-03; // W/(m/s)^3
    double M = 1325.0; // kg

    // power in W
    double P_tract = A*v + B*v*v + C*v*v*v + M*a*v; // for sloped roads: +M*g*sin_theta*v

    /*
    // "Category 7 vehicle" (e.g. a '92 Suzuki Swift)
    double alpha = 1.01;
    double beta = 0.0162;
    double delta = 1.90e-06;
    double zeta = 0.252;
    double alpha1 = 0.985;
    */

    // "Category 9 vehicle" (e.g. a '94 Dodge Spirit)
    double alpha = 1.11;
    double beta = 0.0134;
    double delta = 1.98e-06;
    double zeta = 0.241;
    double alpha1 = 0.973;

    if (P_tract <= 0)
        return alpha1;

    return alpha + beta*v*3.6 + delta*v*v*v*(3.6*3.6*3.6) + zeta*a*v;
}

}
