
#include "Mac1609_4_Mod.h"

Define_Module(Mac1609_4_Mod);

void Mac1609_4_Mod::initialize(int stage)
{
    Mac1609_4::initialize(stage);

	if (stage == 0)
	{

	}
}


void Mac1609_4_Mod::handleSelfMsg(cMessage* msg)
{
    Mac1609_4::handleSelfMsg(msg);


}


void Mac1609_4_Mod::handleUpperControl(cMessage* msg)
{
    Mac1609_4::handleUpperControl(msg);


}


void Mac1609_4_Mod::handleUpperMsg(cMessage* msg)
{
    Mac1609_4::handleUpperMsg(msg);

    MacStatistics();

}


void Mac1609_4_Mod::handleLowerControl(cMessage* msg)
{
    Mac1609_4::handleLowerControl(msg);


}


void Mac1609_4_Mod::handleLowerMsg(cMessage* msg)
{
    Mac1609_4::handleLowerMsg(msg);


}


void Mac1609_4_Mod::MacStatistics()
{
    EV << "####### Mac stats: " << endl;

    EV << "DroppedPackets: " << statsDroppedPackets << endl;                 // packet was dropped in Mac
    EV << "NumTooLittleTime: " << statsNumTooLittleTime << endl;             // Too little time in this interval. Will not schedule nextMacEvent
    EV << "NumInternalContention: " << statsNumInternalContention << endl;   // there was already another packet ready.
                                                                              // we have to go increase cw and go into backoff.
                                                                              // It's called internal contention and its wonderful
    EV << "NumBackoff: " << statsNumBackoff << endl;
    EV << "SlotsBackoff: " << statsSlotsBackoff << endl;
    EV << "TotalBusyTime: " << statsTotalBusyTime << endl;

    EV << "SentPackets: " << statsSentPackets << endl;

    if(statsSNIRLostPackets >= 1)
    {
        double t = simTime().dbl();
        std::string v = getParentModule()->getParentModule()->getFullName();   // v name
        int i = 0;
    }


    EV << "SNIRLostPackets: " << statsSNIRLostPackets << endl;   // A packet was not received due to biterrors
    EV << "TXRXLostPackets: " << statsTXRXLostPackets << endl;   // A packet was not received because we were sending while receiving

    EV << "ReceivedPackets: " << statsReceivedPackets << endl;         // Received a data packet addressed to me
    EV << "ReceivedBroadcasts: " << statsReceivedBroadcasts << endl;   // Received a broadcast data packet
}


void Mac1609_4_Mod::finish()
{
    Mac1609_4::finish();

    //clean up queues.

    // todo: do not let record queue!

}

