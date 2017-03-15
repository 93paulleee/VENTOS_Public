//
// Copyright (C) 2011 David Eckhoff <eckhoff@cs.fau.de>
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

/*
 * Based on PhyLayer.cc from Karl Wessel
 * and modifications by Christopher Saloman
 *
 * Second author: Mani Amoozadeh <maniam@ucdavis.edu>
 */

#include "PhyLayer80211p.h"
#include "Decider80211p.h"
#include "BaseConnectionManager.h"
#include "Consts80211p.h"
#include "AirFrame11p_m.h"
#include "MacToPhyControlInfo.h"
#include "PhyToMacControlInfo.h"
#include "DeciderResult80211.h"

#include "SimplePathlossModel.h"
#include "BreakpointPathlossModel.h"
#include "LogNormalShadowing.h"
#include "JakesFading.h"
#include "PERModel.h"
#include "SimpleObstacleShadowing.h"
#include "TwoRayInterferenceModel.h"
#include "NakagamiFading.h"

namespace Veins {

Define_Module(Veins::PhyLayer80211p);

Coord NoMobiltyPos = Coord::ZERO;

PhyLayer80211p::PhyLayer80211p()
{
    this->protocolId = GENERIC;
    this->emulationActive = false;
    this->thermalNoise = 0;
    this->radio = 0;
    this->decider = 0;
    this->radioSwitchingOverTimer = 0;
    this->txOverTimer = 0;
    this->headerLength = -1;
    this->world = NULL;
}


PhyLayer80211p::~PhyLayer80211p()
{
    //get AirFrames from ChannelInfo and delete
    //(although ChannelInfo normally owns the AirFrames it
    //is not able to cancel and delete them itself
    AirFrameVector channel;
    channelInfo.getAirFrames(0, omnetpp::simTime(), channel);

    for(AirFrameVector::iterator it = channel.begin(); it != channel.end(); ++it)
        cancelAndDelete(*it);

    //free timer messages
    if(txOverTimer)
        cancelAndDelete(txOverTimer);

    if(radioSwitchingOverTimer)
        cancelAndDelete(radioSwitchingOverTimer);

    //free thermal noise mapping
    if(thermalNoise)
        delete thermalNoise;

    //free Decider
    if(decider != 0)
        delete decider;

    /*
     * get a pointer to the radios RSAM again to avoid deleting it,
     * it is not created by calling new (BasePhyLayer is not the owner)!
     */
    AnalogueModel* rsamPointer = radio ? radio->getAnalogueModel() : NULL;

    //free AnalogueModels
    for(AnalogueModelList::iterator it = analogueModels.begin(); it != analogueModels.end(); it++)
    {
        AnalogueModel* tmp = *it;

        // do not delete the RSAM, it's not allocated by new!
        if (tmp == rsamPointer)
        {
            rsamPointer = 0;
            continue;
        }

        if(tmp != 0)
            delete tmp;
    }

    // free radio
    if(radio != 0)
        delete radio;
}


void PhyLayer80211p::initialize(int stage)
{
    ChannelAccess::initialize(stage);

    if (stage == 0)
    {
        //get ccaThreshold before calling BasePhyLayer::initialize() which instantiates the deciders
        ccaThreshold = pow(10, par("ccaThreshold").doubleValue() / 10);
        collectCollisionStatistics = par("collectCollisionStatistics").boolValue();

        // if using sendDirect, make sure that messages arrive without delay
        gate("radioIn")->setDeliverOnReceptionStart(true);

        //get gate ids
        upperLayerIn = findGate("upperLayerIn");
        upperLayerOut = findGate("upperLayerOut");
        upperControlOut = findGate("upperControlOut");
        upperControlIn = findGate("upperControlIn");

        emulationActive = this->getParentModule()->par("emulationActive").boolValue();

        if(par("useThermalNoise").boolValue())
        {
            double thermalNoiseVal = FWMath::dBm2mW(par("thermalNoise").doubleValue());
            thermalNoise = new ConstantSimpleConstMapping(DimensionSet::timeDomain(), thermalNoiseVal);
        }
        else
            thermalNoise = 0;

        sensitivity = FWMath::dBm2mW(par("sensitivity").doubleValue());

        headerLength = par("headerLength").longValue();
        recordStats = par("recordStats").boolValue();

        // initialize radio
        radio = initializeRadio();

        // get pointer to the world module
        world = FindModule<BaseWorldUtility*>::findGlobalModule();
        if (world == NULL)
            throw omnetpp::cRuntimeError("Could not find BaseWorldUtility module");

        if(cc->hasPar("sat") && (sensitivity - FWMath::dBm2mW(cc->par("sat").doubleValue())) < -0.000001)
        {
            throw omnetpp::cRuntimeError("Sensitivity can't be smaller than the "
                    "signal attenuation threshold (sat) in ConnectionManager. "
                    "Please adjust your omnetpp.ini file accordingly.");
        }

        // analogue model parameters
        initializeAnalogueModels(par("analogueModels").xmlValue());

        // decider parameters
        initializeDecider(par("decider").xmlValue());

        // Initialize timer messages
        radioSwitchingOverTimer = new omnetpp::cMessage("radio switching over", RADIO_SWITCHING_OVER);
        txOverTimer = new omnetpp::cMessage("transmission over", TX_OVER);

        if (par("headerLength").longValue() != PHY_HDR_TOTAL_LENGTH)
            throw omnetpp::cRuntimeError("The header length of the 802.11p standard is 46bit, please change your omnetpp.ini accordingly by either setting it to 46bit or removing the entry");

        // erase the RadioStateAnalogueModel
        analogueModels.erase(analogueModels.begin());

        // get a pointer to the Statistics module
        omnetpp::cModule *module = omnetpp::getSimulation()->getSystemModule()->getSubmodule("statistics");
        STAT = static_cast<VENTOS::Statistics*>(module);
        ASSERT(STAT);

        record_stat = par("record_stat").boolValue();
        record_frameTxRx = par("record_frameTxRx").boolValue();

        myId = getParentModule()->getParentModule()->getFullName();
    }
}


void PhyLayer80211p::finish()
{
    // give decider the chance to do something
    decider->finish();
}


void PhyLayer80211p::handleMessage(omnetpp::cMessage* msg)
{
    // self messages
    if(msg->isSelfMessage())
        handleSelfMessage(msg);
    // MacPkts <-- MacToPhyControlInfo
    else if(msg->getArrivalGateId() == upperLayerIn)
        handleUpperMessage(msg);
    // control messages
    else if(msg->getArrivalGateId() == upperControlIn)
        handleUpperControlMessage(msg);
    // AirFrames
    else if(msg->getKind() == AIR_FRAME)
    {
        // get a reference to this node
        omnetpp::cModule *thisNode = this->getParentModule()->getParentModule();
        // make sure that DSRCenabled is true on this node
        if(!thisNode->par("DSRCenabled"))
            throw omnetpp::cRuntimeError("Cannot send msg %s: DSRCenabled parameter is false in %s", msg->getName(), thisNode->getFullName());

        handleAirFrame(static_cast<AirFrame*>(msg));
    }
    // unknown message
    else
    {
        EV << "Unknown message received. \n";
        delete msg;
    }
}


void PhyLayer80211p::handleUpperMessage(omnetpp::cMessage* msg)
{
    // check if Radio is in TX state
    if (radio->getCurrentState() != Radio::TX)
    {
        delete msg;
        msg = 0;

        throw omnetpp::cRuntimeError("Error: message for sending received, but radio not in state TX");
    }

    // check if not already sending
    if(txOverTimer->isScheduled())
    {
        delete msg;
        msg = 0;

        throw omnetpp::cRuntimeError("Error: message for sending received, but radio already sending");
    }

    // build the AirFrame to send
    assert(dynamic_cast<omnetpp::cPacket*>(msg) != 0);

    AirFrame* frame = encapsMsg(static_cast<omnetpp::cPacket*>(msg));

    // make sure there is no self message of kind TX_OVER scheduled
    // and schedule the actual one
    assert (!txOverTimer->isScheduled());
    scheduleAt(omnetpp::simTime() + frame->getDuration(), txOverTimer);

    sendToChannel(frame);

    NumSentFrames++;

    if(record_stat)
        record_PHY_stat_func();
}


void PhyLayer80211p::handleUpperControlMessage(omnetpp::cMessage* msg)
{
    switch(msg->getKind())
    {
    case CHANNEL_SENSE_REQUEST:
        handleChannelSenseRequest(msg);
        break;
    default:
        throw omnetpp::cRuntimeError("Received unknown control message from upper layer!");
        break;
    }
}


Radio* PhyLayer80211p::initializeRadio()
{
    int initialRadioState = par("initialRadioState").longValue();
    double radioMinAtt = par("radioMinAtt").doubleValue();
    double radioMaxAtt = par("radioMaxAtt").doubleValue();
    int nbRadioChannels = hasPar("nbRadioChannels") ? par("nbRadioChannels") : 1;
    int initialRadioChannel = hasPar("initialRadioChannel") ? par("initialRadioChannel") : 0;

    Radio* radio = Radio::createNewRadio(recordStats, initialRadioState,
            radioMinAtt, radioMaxAtt, initialRadioChannel, nbRadioChannels);

    //  - switch times to TX
    //if no RX to TX defined asume same time as sleep to TX
    radio->setSwitchTime(Radio::RX, Radio::TX, (hasPar("timeRXToTX") ? par("timeRXToTX") : par("timeSleepToTX")).doubleValue());
    //if no sleep to TX defined asume same time as RX to TX
    radio->setSwitchTime(Radio::SLEEP, Radio::TX, (hasPar("timeSleepToTX") ? par("timeSleepToTX") : par("timeRXToTX")).doubleValue());

    //  - switch times to RX
    //if no TX to RX defined asume same time as sleep to RX
    radio->setSwitchTime(Radio::TX, Radio::RX, (hasPar("timeTXToRX") ? par("timeTXToRX") : par("timeSleepToRX")).doubleValue());
    //if no sleep to RX defined asume same time as TX to RX
    radio->setSwitchTime(Radio::SLEEP, Radio::RX, (hasPar("timeSleepToRX") ? par("timeSleepToRX") : par("timeTXToRX")).doubleValue());

    //  - switch times to sleep
    //if no TX to sleep defined asume same time as RX to sleep
    radio->setSwitchTime(Radio::TX, Radio::SLEEP, (hasPar("timeTXToSleep") ? par("timeTXToSleep") : par("timeRXToSleep")).doubleValue());
    //if no RX to sleep defined asume same time as TX to sleep
    radio->setSwitchTime(Radio::RX, Radio::SLEEP, (hasPar("timeRXToSleep") ? par("timeRXToSleep") : par("timeTXToSleep")).doubleValue());

    return radio;
}


void PhyLayer80211p::initializeAnalogueModels(omnetpp::cXMLElement* xmlConfig)
{
    /*
     * first of all, attach the AnalogueModel that represents the RadioState
     * to the AnalogueModelList as first element.
     */

    std::string s("RadioStateAnalogueModel");
    ParameterMap p;

    AnalogueModel* newAnalogueModel = getAnalogueModelFromName(s, p);
    if(newAnalogueModel == 0)
        throw omnetpp::cRuntimeError("Could not find an analogue model with the name \"%s\".", s.c_str());

    analogueModels.push_back(newAnalogueModel);

    // then load all the analog models listed in the xml file

    if(xmlConfig == 0)
        throw omnetpp::cRuntimeError("No analogue models configuration file specified.");

    omnetpp::cXMLElementList analogueModelList = xmlConfig->getElementsByTagName("AnalogueModel");
    if(analogueModelList.empty())
        throw omnetpp::cRuntimeError("No analogue models configuration found in configuration file.");

    for(auto &analogueModelData : analogueModelList)
    {
        const char* name = analogueModelData->getAttribute("type");
        if(name == 0)
            throw omnetpp::cRuntimeError("Could not read name of analogue model.");

        ParameterMap params;
        getParametersFromXML(analogueModelData, params);

        AnalogueModel* newAnalogueModel = getAnalogueModelFromName(name, params);
        if(newAnalogueModel == 0)
            throw omnetpp::cRuntimeError("Could not find an analogue model with the name \"%s\".", name);

        // attach the new AnalogueModel to the AnalogueModelList
        analogueModels.push_back(newAnalogueModel);

        coreEV << "AnalogueModel \"" << name << "\" loaded." << std::endl;
    }
}


void PhyLayer80211p::initializeDecider(omnetpp::cXMLElement* xmlConfig)
{
    decider = 0;

    if(xmlConfig == 0)
        throw omnetpp::cRuntimeError("No decider configuration file specified.");

    omnetpp::cXMLElementList deciderList = xmlConfig->getElementsByTagName("Decider");

    if(deciderList.empty())
        throw omnetpp::cRuntimeError("No decider configuration found in configuration file.");

    if(deciderList.size() > 1)
        throw omnetpp::cRuntimeError("More than one decider configuration found in configuration file.");

    omnetpp::cXMLElement* deciderData = deciderList.front();

    const char* name = deciderData->getAttribute("type");

    if(name == 0)
        throw omnetpp::cRuntimeError("Could not read type of decider from configuration file.");

    ParameterMap params;
    getParametersFromXML(deciderData, params);

    decider = getDeciderFromName(name, params);

    if(decider == 0)
        throw omnetpp::cRuntimeError("Could not find a decider with the name \"%s\".", name);

    coreEV << "Decider \"" << name << "\" loaded." << std::endl;
}


void PhyLayer80211p::getParametersFromXML(omnetpp::cXMLElement* xmlData, ParameterMap& outputMap)
{
    omnetpp::cXMLElementList parameters = xmlData->getElementsByTagName("Parameter");

    for(auto &it : parameters)
    {
        const char* name = it->getAttribute("name");
        const char* type = it->getAttribute("type");
        const char* value = it->getAttribute("value");

        if(name == 0 || type == 0 || value == 0)
            throw omnetpp::cRuntimeError("Invalid parameter, could not find name, type or value.");

        std::string sType = type;   //needed for easier comparison
        std::string sValue = value; //needed for easier comparison

        omnetpp::cMsgPar param(name);

        //parse type of parameter and set value
        if (sType == "bool")
            param.setBoolValue(sValue == "true" || sValue == "1");
        else if (sType == "double")
            param.setDoubleValue(strtod(value, 0));
        else if (sType == "string")
            param.setStringValue(value);
        else if (sType == "long")
            param.setLongValue(strtol(value, 0, 0));
        else
            throw omnetpp::cRuntimeError("Unknown parameter type: '%s'", sType.c_str());

        //add parameter to output map
        outputMap[name] = param;
    }
}


AnalogueModel* PhyLayer80211p::getAnalogueModelFromName(std::string name, ParameterMap& params)
{
    if (name == "SimplePathlossModel")
        return initializeSimplePathlossModel(params);
    else if (name == "LogNormalShadowing")
        return initializeLogNormalShadowing(params);
    else if (name == "JakesFading")
        return initializeJakesFading(params);
    else if(name == "BreakpointPathlossModel")
        return initializeBreakpointPathlossModel(params);
    else if(name == "PERModel")
        return initializePERModel(params);
    else if (name == "SimpleObstacleShadowing")
        return initializeSimpleObstacleShadowing(params);
    else if (name == "TwoRayInterferenceModel")
    {
        if (world->use2D())
            throw omnetpp::cRuntimeError("The TwoRayInterferenceModel uses nodes' z-position as the antenna height over ground. Refusing to work in a 2D world");

        return initializeTwoRayInterferenceModel(params);
    }
    else if (name == "NakagamiFading")
        return initializeNakagamiFading(params);
    // case "RSAM", pointer is valid as long as the radio exists
    else if (name == "RadioStateAnalogueModel")
        return radio->getAnalogueModel();

    return 0;
}


AnalogueModel* PhyLayer80211p::initializeLogNormalShadowing(ParameterMap& params)
{
    double mean = params["mean"].doubleValue();
    double stdDev = params["stdDev"].doubleValue();
    omnetpp::simtime_t interval = params["interval"].doubleValue();

    return new LogNormalShadowing(mean, stdDev, interval);
}


AnalogueModel* PhyLayer80211p::initializeJakesFading(ParameterMap& params)
{
    int fadingPaths = params["fadingPaths"].longValue();
    omnetpp::simtime_t delayRMS = params["delayRMS"].doubleValue();
    omnetpp::simtime_t interval = params["interval"].doubleValue();

    double carrierFrequency = 5.890e+9;

    if (params.count("carrierFrequency") > 0)
        carrierFrequency = params["carrierFrequency"];
    else if (cc->hasPar("carrierFrequency"))
        carrierFrequency = cc->par("carrierFrequency").doubleValue();

    return new JakesFading(fadingPaths, delayRMS, carrierFrequency, interval);
}


AnalogueModel* PhyLayer80211p::initializeBreakpointPathlossModel(ParameterMap& params)
{
    double alpha1 = -1, alpha2 = -1, breakpointDistance = -1;
    double L01 = -1, L02 = -1;
    double carrierFrequency = 5.890e+9;
    bool useTorus = world->useTorus();
    const Coord& playgroundSize = *(world->getPgs());
    ParameterMap::iterator it;

    it = params.find("alpha1");
    if ( it != params.end() ) // parameter alpha1 has been specified in config.xml
    {
        // set alpha1
        alpha1 = it->second.doubleValue();
        coreEV << "createPathLossModel(): alpha1 set from config.xml to " << alpha1 << std::endl;
        // check whether alpha is not smaller than specified in ConnectionManager
        if(cc->hasPar("alpha") && alpha1 < cc->par("alpha").doubleValue())
        {
            throw omnetpp::cRuntimeError("TestPhyLayer::createPathLossModel(): alpha can't be smaller than specified in \
	               ConnectionManager. Please adjust your config.xml file accordingly");
        }
    }

    it = params.find("L01");
    if(it != params.end())
        L01 = it->second.doubleValue();

    it = params.find("L02");
    if(it != params.end())
        L02 = it->second.doubleValue();

    it = params.find("alpha2");
    if ( it != params.end() ) // parameter alpha1 has been specified in config.xml
    {
        // set alpha2
        alpha2 = it->second.doubleValue();
        coreEV << "createPathLossModel(): alpha2 set from config.xml to " << alpha2 << std::endl;
        // check whether alpha is not smaller than specified in ConnectionManager
        if(cc->hasPar("alpha") && alpha2 < cc->par("alpha").doubleValue())
        {
            throw omnetpp::cRuntimeError("TestPhyLayer::createPathLossModel(): alpha can't be smaller than specified in \
	               ConnectionManager. Please adjust your config.xml file accordingly");
        }
    }

    it = params.find("breakpointDistance");
    if ( it != params.end() ) // parameter alpha1 has been specified in config.xml
    {
        breakpointDistance = it->second.doubleValue();
        coreEV << "createPathLossModel(): breakpointDistance set from config.xml to " << alpha2 << std::endl;
        // check whether alpha is not smaller than specified in ConnectionManager
    }

    // get carrierFrequency from config
    it = params.find("carrierFrequency");
    if ( it != params.end() ) // parameter carrierFrequency has been specified in config.xml
    {
        // set carrierFrequency
        carrierFrequency = it->second.doubleValue();
        coreEV << "createPathLossModel(): carrierFrequency set from config.xml to " << carrierFrequency << std::endl;

        // check whether carrierFrequency is not smaller than specified in ConnectionManager
        if(cc->hasPar("carrierFrequency") && carrierFrequency < cc->par("carrierFrequency").doubleValue())
        {
            // throw error
            throw omnetpp::cRuntimeError("TestPhyLayer::createPathLossModel(): carrierFrequency can't be smaller than specified in \
	               ConnectionManager. Please adjust your config.xml file accordingly");
        }
    }
    else // carrierFrequency has not been specified in config.xml
    {
        if (cc->hasPar("carrierFrequency")) // parameter carrierFrequency has been specified in ConnectionManager
        {
            // set carrierFrequency according to ConnectionManager
            carrierFrequency = cc->par("carrierFrequency").doubleValue();
            coreEV << "createPathLossModel(): carrierFrequency set from ConnectionManager to " << carrierFrequency << std::endl;
        }
        else // carrierFrequency has not been specified in ConnectionManager
        {
            // keep carrierFrequency at default value
            coreEV << "createPathLossModel(): carrierFrequency set from default value to " << carrierFrequency << std::endl;
        }
    }

    if(alpha1 ==-1 || alpha2==-1 || breakpointDistance==-1 || L01==-1 || L02==-1)
        throw omnetpp::cRuntimeError("Undefined parameters for breakpointPathlossModel. Please check your configuration.");

    return new BreakpointPathlossModel(L01, L02, alpha1, alpha2, breakpointDistance, carrierFrequency, useTorus, playgroundSize, coreDebug);
}


AnalogueModel* PhyLayer80211p::initializeTwoRayInterferenceModel(ParameterMap& params)
{
    ASSERT(params.count("DielectricConstant") == 1);
    double dielectricConstant= params["DielectricConstant"].doubleValue();

    return new TwoRayInterferenceModel(dielectricConstant, coreDebug);
}


AnalogueModel* PhyLayer80211p::initializeNakagamiFading(ParameterMap& params)
{
    bool constM = params["constM"].boolValue();
    double m = 0;
    if (constM)
        m = params["m"].doubleValue();

    return new NakagamiFading(constM, m, coreDebug);
}


AnalogueModel* PhyLayer80211p::initializeSimplePathlossModel(ParameterMap& params)
{
    // init with default value
    double alpha = 2.0;
    double carrierFrequency = 5.890e+9;
    bool useTorus = world->useTorus();
    const Coord& playgroundSize = *(world->getPgs());

    // get alpha-coefficient from config
    ParameterMap::iterator it = params.find("alpha");

    if ( it != params.end() ) // parameter alpha has been specified in config.xml
    {
        // set alpha
        alpha = it->second.doubleValue();
        coreEV << "createPathLossModel(): alpha set from config.xml to " << alpha << std::endl;

        // check whether alpha is not smaller than specified in ConnectionManager
        if(cc->hasPar("alpha") && alpha < cc->par("alpha").doubleValue())
        {
            throw omnetpp::cRuntimeError("TestPhyLayer::createPathLossModel(): alpha can't be smaller than specified in \
	               ConnectionManager. Please adjust your config.xml file accordingly");
        }
    }
    else // alpha has not been specified in config.xml
    {
        if (cc->hasPar("alpha")) // parameter alpha has been specified in ConnectionManager
        {
            // set alpha according to ConnectionManager
            alpha = cc->par("alpha").doubleValue();
            coreEV << "createPathLossModel(): alpha set from ConnectionManager to " << alpha << std::endl;
        }
        else // alpha has not been specified in ConnectionManager
        {
            // keep alpha at default value
            coreEV << "createPathLossModel(): alpha set from default value to " << alpha << std::endl;
        }
    }

    // get carrierFrequency from config
    it = params.find("carrierFrequency");
    if ( it != params.end() ) // parameter carrierFrequency has been specified in config.xml
    {
        // set carrierFrequency
        carrierFrequency = it->second.doubleValue();
        coreEV << "createPathLossModel(): carrierFrequency set from config.xml to " << carrierFrequency << std::endl;

        // check whether carrierFrequency is not smaller than specified in ConnectionManager
        if(cc->hasPar("carrierFrequency") && carrierFrequency < cc->par("carrierFrequency").doubleValue())
        {
            throw omnetpp::cRuntimeError("TestPhyLayer::createPathLossModel(): carrierFrequency can't be smaller than specified in \
	               ConnectionManager. Please adjust your config.xml file accordingly");
        }
    }
    else // carrierFrequency has not been specified in config.xml
    {
        if (cc->hasPar("carrierFrequency")) // parameter carrierFrequency has been specified in ConnectionManager
        {
            // set carrierFrequency according to ConnectionManager
            carrierFrequency = cc->par("carrierFrequency").doubleValue();
            coreEV << "createPathLossModel(): carrierFrequency set from ConnectionManager to " << carrierFrequency << std::endl;
        }
        else // carrierFrequency has not been specified in ConnectionManager
        {
            // keep carrierFrequency at default value
            coreEV << "createPathLossModel(): carrierFrequency set from default value to " << carrierFrequency << std::endl;
        }
    }

    return new SimplePathlossModel(alpha, carrierFrequency, useTorus, playgroundSize, coreDebug);
}


AnalogueModel* PhyLayer80211p::initializePERModel(ParameterMap& params)
{
    double per = params["packetErrorRate"].doubleValue();
    return new PERModel(per);
}


Decider* PhyLayer80211p::getDeciderFromName(std::string name, ParameterMap& params)
{
    if(name == "Decider80211p")
    {
        protocolId = IEEE_80211;
        return initializeDecider80211p(params);
    }

    return 0;
}


AnalogueModel* PhyLayer80211p::initializeSimpleObstacleShadowing(ParameterMap& params)
{
    // init with default value
    double carrierFrequency = 5.890e+9;
    bool useTorus = world->useTorus();
    const Coord& playgroundSize = *(world->getPgs());

    ParameterMap::iterator it;

    // get carrierFrequency from config
    it = params.find("carrierFrequency");

    if ( it != params.end() ) // parameter carrierFrequency has been specified in config.xml
    {
        // set carrierFrequency
        carrierFrequency = it->second.doubleValue();
        coreEV << "initializeSimpleObstacleShadowing(): carrierFrequency set from config.xml to " << carrierFrequency << std::endl;

        // check whether carrierFrequency is not smaller than specified in ConnectionManager
        if(cc->hasPar("carrierFrequency") && carrierFrequency < cc->par("carrierFrequency").doubleValue())
            throw omnetpp::cRuntimeError("initializeSimpleObstacleShadowing(): carrierFrequency can't be smaller than specified in ConnectionManager. Please adjust your config.xml file accordingly");
    }
    else // carrierFrequency has not been specified in config.xml
    {
        if (cc->hasPar("carrierFrequency")) // parameter carrierFrequency has been specified in ConnectionManager
        {
            // set carrierFrequency according to ConnectionManager
            carrierFrequency = cc->par("carrierFrequency").doubleValue();
            coreEV << "createPathLossModel(): carrierFrequency set from ConnectionManager to " << carrierFrequency << std::endl;
        }
        else // carrierFrequency has not been specified in ConnectionManager
        {
            // keep carrierFrequency at default value
            coreEV << "createPathLossModel(): carrierFrequency set from default value to " << carrierFrequency << std::endl;
        }
    }

    ObstacleControl* obstacleControlP = ObstacleControlAccess().getIfExists();
    if (!obstacleControlP)
        throw omnetpp::cRuntimeError("initializeSimpleObstacleShadowing(): cannot find ObstacleControl module");

    return new SimpleObstacleShadowing(*obstacleControlP, carrierFrequency, useTorus, playgroundSize, coreDebug);
}


Decider* PhyLayer80211p::initializeDecider80211p(ParameterMap& params)
{
    double centerFreq = params["centerFrequency"];
    Decider80211p* dec = new Decider80211p(this, sensitivity, ccaThreshold, allowTxDuringRx, centerFreq, findHost()->getIndex(), collectCollisionStatistics, coreDebug);
    dec->setPath(getParentModule()->getFullPath());
    return dec;
}


void PhyLayer80211p::changeListeningFrequency(double freq)
{
    Decider80211p* dec = dynamic_cast<Decider80211p*>(decider);
    assert(dec);

    dec->changeFrequency(freq);
}


void PhyLayer80211p::handleSelfMessage(omnetpp::cMessage* msg)
{
    switch(msg->getKind())
    {
    // transmission over
    case TX_OVER:
    {
        assert(msg == txOverTimer);

        sendControlMsgToMac(new VENTOS::PhyToMacReport("Transmission over", TX_OVER));

        Decider80211p* dec = dynamic_cast<Decider80211p*>(decider);
        assert(dec);

        // check if there is another packet on the chan
        if (dec->cca(omnetpp::simTime(),NULL))
        {
            // change the chan-state to idle
            DBG << "Channel idle after transmit!\n";
            dec->setChannelIdleStatus(true);
        }
        else
            DBG << "Channel not yet idle after transmit! \n";

        break;
    }
    // radio switch over
    case RADIO_SWITCHING_OVER:
        assert(msg == radioSwitchingOverTimer);
        finishRadioSwitching();
        break;

        //AirFrame
    case AIR_FRAME:
        handleAirFrame(static_cast<AirFrame*>(msg));
        break;

        //ChannelSenseRequest
    case CHANNEL_SENSE_REQUEST:
        handleChannelSenseRequest(msg);
        break;

    default:
        break;
    }
}


void PhyLayer80211p::handleAirFrame(AirFrame* frame)
{
    //TODO: ask jerome to set air frame priority in his UWBIRPhy
    //assert(frame->getSchedulingPriority() == airFramePriority());

    switch(frame->getState())
    {
    case START_RECEIVE:
        handleAirFrameStartReceive(frame);
        break;
    case RECEIVING:
        handleAirFrameReceiving(frame);
        break;
    case END_RECEIVE:
        handleAirFrameEndReceive(frame);
        break;
    default:
        throw omnetpp::cRuntimeError("Unknown AirFrame state: %s", frame->getState());
        break;
    }
}


void PhyLayer80211p::handleAirFrameStartReceive(AirFrame* frame)
{
    coreEV << "Received new AirFrame " << frame << " from channel. \n";

    if(channelInfo.isChannelEmpty())
        radio->setTrackingModeTo(true);

    channelInfo.addAirFrame(frame, omnetpp::simTime());
    assert(!channelInfo.isChannelEmpty());

    if(usePropagationDelay)
    {
        Signal& s = frame->getSignal();
        omnetpp::simtime_t delay = omnetpp::simTime() - s.getSendingStart();
        s.setPropagationDelay(delay);
    }

    // sendingStart + propagationDelay
    assert(frame->getSignal().getReceptionStart() == omnetpp::simTime());

    if(emulationActive)
    {
        frame->getSignal().setReceptionSenderInfo(frame);
        filterSignal(frame);

        if(decider && isKnownProtocolId(frame->getProtocolId()))
        {
            frame->setState(RECEIVING);

            //pass the AirFrame the first time to the Decider
            handleAirFrameReceiving(frame);
        }
        //if no decider is defined we will schedule the message directly to its end
        else
        {
            Signal& signal = frame->getSignal();

            omnetpp::simtime_t signalEndTime = signal.getReceptionStart() + frame->getDuration();
            frame->setState(END_RECEIVE);

            scheduleAt(signalEndTime, frame);
        }
    }
    else
    {
        // set frame state to 'END_RECEIVE'
        frame->setState(END_RECEIVE);

        // schedule the message directly to its end
        Signal& signal = frame->getSignal();
        omnetpp::simtime_t signalEndTime = signal.getReceptionStart() + frame->getDuration();
        scheduleAt(signalEndTime, frame);

        return;
    }
}


void PhyLayer80211p::handleAirFrameReceiving(AirFrame* frame)
{
    Signal& signal = frame->getSignal();
    omnetpp::simtime_t nextHandleTime = decider->processSignal(frame);

    assert(signal.getDuration() == frame->getDuration());
    omnetpp::simtime_t signalEndTime = signal.getReceptionStart() + frame->getDuration();

    //check if this is the end of the receiving process
    if(omnetpp::simTime() >= signalEndTime)
    {
        frame->setState(END_RECEIVE);
        handleAirFrameEndReceive(frame);
        return;
    }

    //smaller zero means don't give it to me again
    if(nextHandleTime < 0)
    {
        nextHandleTime = signalEndTime;
        frame->setState(END_RECEIVE);
    }
    //invalid point in time
    else if(nextHandleTime < omnetpp::simTime() || nextHandleTime > signalEndTime)
        throw omnetpp::cRuntimeError("Invalid next handle time returned by Decider. Expected a value between current simulation time (%.2f) and end of signal (%.2f) but got %.2f",
                SIMTIME_DBL(omnetpp::simTime()), SIMTIME_DBL(signalEndTime), SIMTIME_DBL(nextHandleTime));

    coreEV << "Handed AirFrame with ID " << frame->getId() << " to Decider. Next handling in " << nextHandleTime - omnetpp::simTime() << "s." << std::endl;

    scheduleAt(nextHandleTime, frame);
}


void PhyLayer80211p::handleAirFrameEndReceive(AirFrame* frame)
{
    if(!emulationActive)
    {
        // emulation is not active, sending the frame up to the MAC
        DeciderResult *result = new DeciderResult80211(false, 0, 0, 0);
        sendUp(frame, result);
    }

    coreEV << "End of Airframe with ID " << frame->getId() << "." << std::endl;

    omnetpp::simtime_t earliestInfoPoint = channelInfo.removeAirFrame(frame);

    /* clean information in the radio until earliest time-point
     *  of information in the ChannelInfo,
     *  since this time-point might have changed due to removal of
     *  the AirFrame
     */
    if(channelInfo.isChannelEmpty())
    {
        earliestInfoPoint = omnetpp::simTime();
        radio->setTrackingModeTo(false);
    }

    radio->cleanAnalogueModelUntil(earliestInfoPoint);
}


void PhyLayer80211p::handleChannelSenseRequest(omnetpp::cMessage* msg)
{
    MacToPhyCSR* senseReq = static_cast<MacToPhyCSR*>(msg);

    omnetpp::simtime_t nextHandleTime = decider->handleChannelSenseRequest(senseReq);

    //schedule request for next handling
    if(nextHandleTime >= omnetpp::simTime())
    {
        scheduleAt(nextHandleTime, msg);

        //don't throw away any AirFrames while ChannelSenseRequest is active
        if(!channelInfo.isRecording())
            channelInfo.startRecording(omnetpp::simTime());
    }
    else if(nextHandleTime >= 0.0)
        throw omnetpp::cRuntimeError("Next handle time of ChannelSenseRequest returned by the Decider is smaller then current simulation time: %.2f",
                SIMTIME_DBL(nextHandleTime));

    // else, i.e. nextHandleTime < 0.0, the Decider doesn't want to handle
    // the request again
}


void PhyLayer80211p::filterSignal(AirFrame *frame)
{
    if (analogueModels.empty())
        return;

    ChannelAccess *const senderModule   = dynamic_cast<ChannelAccess *const>(frame->getSenderModule());
    ChannelAccess *const receiverModule = dynamic_cast<ChannelAccess *const>(frame->getArrivalModule());

    assert(senderModule);
    assert(receiverModule);

    /** claim the Move pattern of the sender from the Signal */
    ChannelMobilityPtrType sendersMobility  = senderModule   ? senderModule->getMobilityModule()   : NULL;
    ChannelMobilityPtrType receiverMobility = receiverModule ? receiverModule->getMobilityModule() : NULL;

    const Coord sendersPos  = sendersMobility  ? sendersMobility->getCurrentPosition() : NoMobiltyPos;
    const Coord receiverPos = receiverMobility ? receiverMobility->getCurrentPosition(): NoMobiltyPos;

    for(auto &it : analogueModels)
        it->filterSignal(frame, sendersPos, receiverPos);
}


AirFrame *PhyLayer80211p::encapsMsg(omnetpp::cPacket *macPkt)
{
    if(!emulationActive)
    {
        // create the new AirFrame11p
        AirFrame* frame = new AirFrame11p(macPkt->getName(), AIR_FRAME);
        frame->encapsulate(macPkt);
        return frame;
    }

    // the macPkt must always have a ControlInfo attached (contains Signal)
    cObject* ctrlInfo = macPkt->removeControlInfo();
    assert(ctrlInfo);

    // Retrieve the pointer to the Signal-instance from the ControlInfo-instance.
    // We are now the new owner of this instance.
    Signal* s = MacToPhyControlInfo::getSignalFromControlInfo(ctrlInfo);
    // make sure we really obtained a pointer to an instance
    assert(s);

    // create the new AirFrame11p
    AirFrame* frame = new AirFrame11p(macPkt->getName(), AIR_FRAME);

    assert(s->getDuration() > 0);
    frame->setDuration(s->getDuration());
    // copy the signal into the AirFrame
    frame->setSignal(*s);
    // set priority of AirFrames above the normal priority to ensure
    // channel consistency (before any thing else happens at a time
    // point t make sure that the channel has removed every AirFrame
    // ended at t and added every AirFrame started at t)
    frame->setSchedulingPriority(airFramePriority());
    frame->setProtocolId(myProtocolId());
    frame->setBitLength(headerLength);
    frame->setId(world->getUniqueAirFrameId());
    frame->setChannel(radio->getCurrentChannel());

    // pointer and Signal not needed anymore
    delete s;
    s = 0;

    // delete the Control info
    delete ctrlInfo;
    ctrlInfo = 0;

    frame->encapsulate(macPkt);

    // from here on, the AirFrame is the owner of the MacPacket
    macPkt = 0;
    coreEV <<"AirFrame encapsulated, length: " << frame->getBitLength() << "\n";

    return frame;
}


void PhyLayer80211p::finishRadioSwitching()
{
    radio->endSwitch(omnetpp::simTime());
    sendControlMsgToMac(new VENTOS::PhyToMacReport("Radio switching over", RADIO_SWITCHING_OVER));
}


int PhyLayer80211p::getRadioState()
{
    Enter_Method_Silent();

    assert(radio);
    return radio->getCurrentState();
}


omnetpp::simtime_t PhyLayer80211p::setRadioState(int rs)
{
    Enter_Method_Silent();

    assert(radio);

    if (rs == Radio::TX)
        decider->switchToTx();

    if(txOverTimer && txOverTimer->isScheduled())
        EV_WARN << "Switched radio while sending an AirFrame. The effects this would have on the transmission are not simulated by the BasePhyLayer!";

    omnetpp::simtime_t switchTime = radio->switchTo(rs, omnetpp::simTime());

    //invalid switch time, we are probably already switching
    if(switchTime < 0)
        return switchTime;

    // if switching is done in exactly zero-time no extra self-message is scheduled
    if (switchTime == 0.0)
    {
        // TODO: in case of zero-time-switch, send no control-message to mac!
        // maybe call a method finishRadioSwitchingSilent()
        finishRadioSwitching();
    }
    else
        scheduleAt(omnetpp::simTime() + switchTime, radioSwitchingOverTimer);

    return switchTime;
}


int PhyLayer80211p::getPhyHeaderLength()
{
    Enter_Method_Silent();

    if (headerLength < 0)
        return par("headerLength").longValue();

    return headerLength;
}


void PhyLayer80211p::setCurrentRadioChannel(int newRadioChannel)
{
    if(txOverTimer && txOverTimer->isScheduled())
        EV_WARN << "Switched channel while sending an AirFrame. The effects this would have on the transmission are not simulated by the BasePhyLayer!";

    radio->setCurrentChannel(newRadioChannel);
    decider->channelChanged(newRadioChannel);
    coreEV << "Switched radio to channel " << newRadioChannel << std::endl;
}


int PhyLayer80211p::getCurrentRadioChannel()
{
    return radio->getCurrentChannel();
}


int PhyLayer80211p::getNbRadioChannels()
{
    return par("nbRadioChannels");
}


ChannelState PhyLayer80211p::getChannelState()
{
    Enter_Method_Silent();

    assert(decider);
    return decider->getChannelState();
}


void PhyLayer80211p::setCCAThreshold(double ccaThreshold_dBm)
{
    ccaThreshold = pow(10, ccaThreshold_dBm / 10);
    ((Decider80211p *)decider)->setCCAThreshold(ccaThreshold_dBm);
}


double PhyLayer80211p::getCCAThreshold()
{
    return 10 * log10(ccaThreshold);
}


void PhyLayer80211p::sendControlMsgToMac(VENTOS::PhyToMacReport* msg)
{
    if(msg->getKind() == CHANNEL_SENSE_REQUEST)
    {
        if(channelInfo.isRecording())
            channelInfo.stopRecording();
    }
    else if (msg->getKind() == Decider80211p::BITERROR)
    {
        NumLostFrames_BiteError++;

        if(record_stat) record_PHY_stat_func();
        if(record_frameTxRx) record_frameTxRx_stat_func(msg, "BITERROR");
    }
    else if(msg->getKind() == Decider80211p::COLLISION)
    {
        NumLostFrames_Collision++;

        if(record_stat) record_PHY_stat_func();
        if(record_frameTxRx) record_frameTxRx_stat_func(msg, "COLLISION");
    }
    else if(msg->getKind() == Decider80211p::RECWHILESEND)
    {
        NumLostFrames_TXRX++;

        if(record_stat) record_PHY_stat_func();
        if(record_frameTxRx) record_frameTxRx_stat_func(msg, "RECWHILESEND");
    }

    send(msg, upperControlOut);
}


void PhyLayer80211p::sendUp(AirFrame* frame, DeciderResult* result)
{
    if(record_frameTxRx)
    {
        // we need the id of the message assigned by OMNET++
        long int frameId = (dynamic_cast<omnetpp::cPacket *>(frame))->getId();
        long int nicId = this->getParentModule()->getId();

        auto it = STAT->global_frameTxRx_stat.find(std::make_pair(frameId, nicId));
        if(it == STAT->global_frameTxRx_stat.end())
            throw omnetpp::cRuntimeError("received frame '%d' has never been transmitted!", frameId);

        it->second.ReceivedAt = omnetpp::simTime().dbl();
        it->second.FrameRxStatus = "HEALTHY";
    }

    NumReceivedFrames++;

    if(record_stat)
        record_PHY_stat_func();

    coreEV << "Decapsulating MacPacket from Airframe with ID " << frame->getId() << " and sending it up to MAC." << std::endl;

    omnetpp::cMessage* packet = frame->decapsulate();
    assert(packet);

    setUpControlInfo(packet, result);

    send(packet, upperLayerOut);
}


void PhyLayer80211p::cancelScheduledMessage(omnetpp::cMessage* msg)
{
    if(msg->isScheduled())
        cancelEvent(msg);
    else
    {
        EV << "Warning: Decider wanted to cancel a scheduled message but message"
                << " wasn't actually scheduled. Message is: " << msg << std::endl;
    }
}


BaseWorldUtility* PhyLayer80211p::getWorldUtility()
{
    return world;
}


void PhyLayer80211p::recordScalar(const char *name, double value, const char *unit)
{
    ChannelAccess::recordScalar(name, value, unit);
}


void PhyLayer80211p::getChannelInfo(omnetpp::simtime_t_cref from, omnetpp::simtime_t_cref to, AirFrameVector& out)
{
    channelInfo.getAirFrames(from, to, out);
}


ConstMapping* PhyLayer80211p::getThermalNoise(omnetpp::simtime_t_cref from, omnetpp::simtime_t_cref to)
{
    if(thermalNoise)
        thermalNoise->initializeArguments(Argument(from));

    return thermalNoise;
}


/**
 * Attaches a "control info" (PhyToMac) structure (object) to the message pMsg.
 */
omnetpp::cObject *const PhyLayer80211p::setUpControlInfo(omnetpp::cMessage *const pMsg, DeciderResult *const pDeciderResult)
{
    return PhyToMacControlInfo::setControlInfo(pMsg, pDeciderResult);
}


void PhyLayer80211p::rescheduleMessage(omnetpp::cMessage* msg, omnetpp::simtime_t_cref t)
{
    cancelScheduledMessage(msg);
    scheduleAt(t, msg);
}


void PhyLayer80211p::record_PHY_stat_func()
{
    VENTOS::PHY_stat_t entry = {};

    entry.last_stat_time = omnetpp::simTime().dbl();
    entry.NumSentFrames = NumSentFrames;
    entry.NumReceivedFrames = NumReceivedFrames;
    entry.NumLostFrames_BiteError = NumLostFrames_BiteError;
    entry.NumLostFrames_Collision = NumLostFrames_Collision;
    entry.NumLostFrames_TXRX = NumLostFrames_TXRX;

    auto it = STAT->global_PHY_stat.find(myId);
    if(it == STAT->global_PHY_stat.end())
        STAT->global_PHY_stat[myId] = entry;
    else
        it->second = entry;
}


void PhyLayer80211p::record_frameTxRx_stat_func(VENTOS::PhyToMacReport* msg, std::string report)
{
    VENTOS::PhyToMacReport *phyReport = dynamic_cast<VENTOS::PhyToMacReport *>(msg);
    ASSERT(phyReport);

    long int frameId = phyReport->getMsgId();
    long int nicId = this->getParentModule()->getId();

    auto it = STAT->global_frameTxRx_stat.find(std::make_pair(frameId, nicId));
    if(it == STAT->global_frameTxRx_stat.end())
        throw omnetpp::cRuntimeError("received frame '%d' has never been transmitted!", frameId);

    it->second.ReceivedAt = omnetpp::simTime().dbl();
    it->second.FrameRxStatus = report;
}

}
