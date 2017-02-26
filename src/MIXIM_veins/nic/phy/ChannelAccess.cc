/***************************************************************************
 * file:        ChannelAccess.cc
 *
 * author:      Marc Loebbers
 *

 * copyright:   (C) 2004 Telecommunication Networks Group (TKN) at
 *              Technische Universitaet Berlin, Germany.
 *
 *              This program is free software; you can redistribute it
 *              and/or modify it under the terms of the GNU General Public
 *              License as published by the Free Software Foundation; either
 *              version 2 of the License, or (at your option) any later
 *              version.
 *              For further information see file COPYING
 *              in the top level directory
 ***************************************************************************
 * part of:     framework implementation developed by tkn
 * description: - Base class for physical layers
 *              - if you create your own physical layer, please subclass
 *                from this class and use the sendToChannel() function!!
 ***************************************************************************
 * changelog:   $Revision: 284 $
 *              last modified:   $Date: 2006-06-07 16:55:24 +0200 (Mi, 07 Jun 2006) $
 *              by:              $Author: willkomm $
 **************************************************************************/

#include <cassert>

#include "ChannelAccess.h"
#include "FindModule.h"
#include "BaseWorldUtility.h"
#include "BaseConnectionManager.h"
#include "AirFrame11p_m.h"
#include "MacToPhyControlInfo.h"

const simsignalwrap_t ChannelAccess::mobilityStateChangedSignal = simsignalwrap_t(MIXIM_SIGNAL_MOBILITY_CHANGE_NAME);

BaseConnectionManager* ChannelAccess::getConnectionManager(cModule* nic)
{
    std::string cmName = nic->hasPar("connectionManagerName") ? nic->par("connectionManagerName").stringValue() : "";
    if (cmName != "")
    {
        omnetpp::cModule* ccModule = omnetpp::cSimulation::getActiveSimulation()->getModuleByPath(cmName.c_str());
        return dynamic_cast<BaseConnectionManager *>(ccModule);
    }
    else
        return FindModule<BaseConnectionManager *>::findGlobalModule();
}


void ChannelAccess::initialize(int stage)
{
    BaseModule::initialize(stage);

    if(stage == 0)
    {
        coreDebug = hasPar("coreDebug") ? par("coreDebug").boolValue() : false;

        findHost()->subscribe(mobilityStateChangedSignal, this);

        cModule* nic = getParentModule();
        cc = getConnectionManager(nic);
        if(cc == NULL)
            throw omnetpp::cRuntimeError("Could not find connectionmanager module");

        isRegistered = false;

        // get a pointer to the Statistics module
        omnetpp::cModule *module = omnetpp::getSimulation()->getSystemModule()->getSubmodule("statistics");
        STAT = static_cast<VENTOS::Statistics*>(module);
        ASSERT(STAT);
    }
    else if (stage == 1)
    {
        record_stat = this->par("record_stat").boolValue();
    }

    usePropagationDelay = par("usePropagationDelay");
}


void ChannelAccess::receiveSignal(omnetpp::cComponent *source, omnetpp::simsignal_t signalID, omnetpp::cObject *obj, cObject* details)
{
    if(signalID == mobilityStateChangedSignal)
    {
        ChannelMobilityPtrType const mobility = omnetpp::check_and_cast<ChannelMobilityPtrType>(obj);
        Coord pos = mobility->getCurrentPosition();

        if(isRegistered)
            // getParentModule points to the 'nic' module
            cc->updateNicPos(getParentModule()->getId(), &pos);
        else
        {
            // register the nic with ConnectionManager
            // returns true, if sendDirect is used
            useSendDirect = cc->registerNic(getParentModule(), this, &pos);
            isRegistered  = true;
        }
    }
}


void ChannelAccess::sendToChannel(omnetpp::cPacket *msg)
{
    const NicEntry::GateList& gateList = cc->getGateList(getParentModule()->getId());
    NicEntry::GateList::const_iterator i = gateList.begin();

    if(useSendDirect)
    {
        // use Andras stuff
        if( i != gateList.end() )
        {
            omnetpp::simtime_t delay = SIMTIME_ZERO;

            for(; i != --gateList.end(); ++i)
            {
                // calculate propagation delay to this receiving nic
                delay = calculatePropagationDelay(i->first);

                int radioStart = i->second->getId();
                int radioEnd = radioStart + i->second->size();
                for (int g = radioStart; g != radioEnd; ++g)
                {
                    omnetpp::cPacket *msg_dup = msg->dup();
                    if(record_stat) recordFrameTx(msg_dup, i->second, delay);
                    sendDirect(static_cast<omnetpp::cPacket*>(msg_dup), delay, msg->getDuration(), i->second->getOwnerModule(), g);
                }
            }

            // calculate Propagation delay to this receiving nic
            delay = calculatePropagationDelay(i->first);

            int radioStart = i->second->getId();
            int radioEnd = radioStart + i->second->size();
            for (int g = radioStart; g != --radioEnd; ++g)
            {
                omnetpp::cPacket *msg_dup = msg->dup();
                if(record_stat) recordFrameTx(msg_dup, i->second, delay);
                sendDirect(static_cast<omnetpp::cPacket*>(msg_dup), delay, msg->getDuration(), i->second->getOwnerModule(), g);
            }

            if(record_stat) recordFrameTx(msg, i->second, delay);
            sendDirect(msg, delay, msg->getDuration(), i->second->getOwnerModule(), radioEnd);
        }
        else
        {
            coreEV << "Nic is not connected to any gates!" << std::endl;
            delete msg;
        }
    }
    else
    {
        // use our stuff
        coreEV << "sendToChannel: sending to gates \n";
        if( i != gateList.end() )
        {
            omnetpp::simtime_t delay = SIMTIME_ZERO;

            for(; i != --gateList.end(); ++i)
            {
                // calculate Propagation delay to this receiving nic
                delay = calculatePropagationDelay(i->first);

                sendDelayed( static_cast<omnetpp::cPacket*>(msg->dup()), delay, i->second );
            }

            // calculate Propagation delay to this receiving nic
            delay = calculatePropagationDelay(i->first);

            sendDelayed( msg, delay, i->second );
        }
        else
        {
            coreEV << "Nic is not connected to any gates!" << std::endl;
            delete msg;
        }
    }
}


omnetpp::simtime_t ChannelAccess::calculatePropagationDelay(const NicEntry* nic)
{
    if(!usePropagationDelay)
        return 0;

    ChannelAccess *const senderModule   = this;
    ChannelAccess *const receiverModule = nic->chAccess;

    assert(senderModule);
    assert(receiverModule);

    // claim the Move pattern of the sender from the Signal
    Coord sendersPos  = senderModule->getMobilityModule()->getCurrentPosition();
    Coord receiverPos = receiverModule->getMobilityModule()->getCurrentPosition();

    // this time-point is used to calculate the distance between sending and receiving host
    return receiverPos.distance(sendersPos) / BaseWorldUtility::speedOfLight();
}


void ChannelAccess::recordFrameTx(omnetpp::cPacket *msg /*AirFrame11p*/, omnetpp::cGate *gate, omnetpp::simtime_t propDelay)
{
    // only SendDirect is supported
    ASSERT(useSendDirect);

    long int frameId = msg->getId();  // unique message id assigned by OMNET++
    long int nicId = gate->getOwnerModule()->getSubmodule("nic")->getId();

    auto it = STAT->global_msgTxRx_stat.find(std::make_pair(frameId, nicId));
    if(it != STAT->global_msgTxRx_stat.end())
        throw omnetpp::cRuntimeError("frame/nic '(%d,%d)' is not unique", frameId, nicId);
    else
    {
        // extract signal from frame
        Veins::AirFrame11p *frame = dynamic_cast<Veins::AirFrame11p *>(msg);
        ASSERT(frame);
        auto signal = frame->getSignal();

        VENTOS::msgTxRxStat_t entry = {};

        entry.msgName = msg->getFullName();
        entry.senderNode = this->getParentModule()->getParentModule()->getFullName();
        entry.receiverNode = gate->getOwnerModule()->getFullName();
        entry.frameSize = msg->getBitLength();
        entry.sentAt = omnetpp::simTime().dbl();
        entry.TxSpeed = 6; //signal.getBitrate();
        entry.TxTime = signal.getDuration().dbl();
        entry.propagationDelay = propDelay.dbl();

        STAT->global_msgTxRx_stat.insert(std::make_pair(std::make_pair(frameId, nicId), entry));
    }
}
