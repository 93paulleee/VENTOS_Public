
#include <Statistics.h>
#include <ApplRSU_03_Manager.h>
#include "Router.h"

namespace VENTOS {

Define_Module(VENTOS::Statistics);

class ApplRSUManager;


Statistics::~Statistics()
{

}


void Statistics::initialize(int stage)
{
    if(stage == 0)
	{
        // get a pointer to the TraCI module
        cModule *module = simulation.getSystemModule()->getSubmodule("TraCI");
        TraCI = static_cast<TraCI_Extend *>(module);
        terminate = module->par("terminate").doubleValue();
        updateInterval = module->par("updateInterval").doubleValue();

        collectMAClayerData = par("collectMAClayerData").boolValue();
        collectVehiclesData = par("collectVehiclesData").boolValue();
        collectInductionLoopData = par("collectInductionLoopData").boolValue();
        collectPlnManagerData = par("collectPlnManagerData").boolValue();
        printBeaconsStatistics = par("printBeaconsStatistics").boolValue();
        printIncidentDetection = par("printIncidentDetection").boolValue();
        LaneCostsMode = par("LaneCostsMode").longValue();
        HysteresisCount = par("HysteresisCount").longValue();

        index = 1;

        parseHistogramFile();

        // register signals
        Signal_executeEachTS = registerSignal("executeEachTS");

        Signal_beaconP = registerSignal("beaconP");
        Signal_beaconO = registerSignal("beaconO");
        Signal_beaconD = registerSignal("beaconD");

        Signal_MacStats = registerSignal("MacStats");

        Signal_SentPlatoonMsg = registerSignal("SentPlatoonMsg");
        Signal_VehicleState = registerSignal("VehicleState");
        Signal_PlnManeuver = registerSignal("PlnManeuver");

        // now subscribe locally to all these signals
        simulation.getSystemModule()->subscribe("executeEachTS", this);
        simulation.getSystemModule()->subscribe("beaconP", this);
        simulation.getSystemModule()->subscribe("beaconO", this);
        simulation.getSystemModule()->subscribe("beaconD", this);
        simulation.getSystemModule()->subscribe("MacStats", this);
        simulation.getSystemModule()->subscribe("SentPlatoonMsg", this);
        simulation.getSystemModule()->subscribe("VehicleState", this);
        simulation.getSystemModule()->subscribe("PlnManeuver", this);
    }
}


void Statistics::finish()
{
    if(LaneCostsMode == 1)
        HistogramsToFile();
}


void Statistics::handleMessage(cMessage *msg)
{


}


// todo: make all beacons object
void Statistics::receiveSignal(cComponent *source, simsignal_t signalID, long i)
{
    Enter_Method_Silent();

    EV << "*** Statistics module received signal " << signalID;
    EV << " from module " << source->getFullName();
    EV << " with value " << i << endl;

    int nodeIndex = getNodeIndex(source->getFullName());

    if(signalID == Signal_executeEachTS)
    {
        Statistics::executeEachTimestep(i);
    }
    else if(signalID == Signal_beaconD)
    {
        // from preceding
        if(i==1)
        {
            NodeEntry *tmp = new NodeEntry(source->getFullName(), "-", nodeIndex, -1, simTime());
            Vec_BeaconsDP.push_back(tmp);
        }
        // from other vehicles
        else if(i==2)
        {
            NodeEntry *tmp = new NodeEntry(source->getFullName(), "-", nodeIndex, -1, simTime());
            Vec_BeaconsDO.push_back(tmp);
        }
    }
}


void Statistics::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj)
{
    Enter_Method_Silent();

    EV << "*** Statistics module received signal " << signalID;
    EV << " from module " << source->getFullName() << endl;

    int nodeIndex = getNodeIndex(source->getFullName());

    if(collectMAClayerData && signalID == Signal_MacStats)
    {
        MacStat *m = static_cast<MacStat *>(obj);
        if (m == NULL) return;

        int counter = findInVector(Vec_MacStat, source->getFullName());

        // its a new entry, so we add it.
        if(counter == -1)
        {
            MacStatEntry *tmp = new MacStatEntry(source->getFullName(), nodeIndex, simTime().dbl(), m->vec);
            Vec_MacStat.push_back(tmp);
        }
        // if found, just update the existing fields
        else
        {
            Vec_MacStat[counter]->time = simTime().dbl();
            Vec_MacStat[counter]->MacStatsVec = m->vec;
        }
    }
    else if(collectPlnManagerData && signalID == Signal_VehicleState)
    {
        CurrentVehicleState *state = dynamic_cast<CurrentVehicleState*>(obj);
        ASSERT(state);

        plnManagement *tmp = new plnManagement(simTime().dbl(), state->name, "-", state->state, "-", "-");
        Vec_plnManagement.push_back(tmp);
    }
    else if(collectPlnManagerData && signalID == Signal_SentPlatoonMsg)
    {
        CurrentPlnMsg* plnMsg = dynamic_cast<CurrentPlnMsg*>(obj);
        ASSERT(plnMsg);

        plnManagement *tmp = new plnManagement(simTime().dbl(), plnMsg->msg->getSender(), plnMsg->msg->getRecipient(), plnMsg->type, plnMsg->msg->getSendingPlatoonID(), plnMsg->msg->getReceivingPlatoonID());
        Vec_plnManagement.push_back(tmp);
    }
    else if(collectPlnManagerData && signalID == Signal_PlnManeuver)
    {
        PlnManeuver* com = dynamic_cast<PlnManeuver*>(obj);
        ASSERT(com);

        plnStat *tmp = new plnStat(simTime().dbl(), com->from, com->to, com->maneuver);
        Vec_plnStat.push_back(tmp);
    }

    // todo
    else if(signalID == Signal_beaconP)
    {
        data *m = static_cast<data *>(obj);
        if (m == NULL) return;

        NodeEntry *tmp = new NodeEntry(source->getFullName(), m->name, nodeIndex, -1, simTime());
        Vec_BeaconsP.push_back(tmp);
    }
    else if(signalID == Signal_beaconO)
    {
        data *m = static_cast<data *>(obj);
        if (m == NULL) return;

        NodeEntry *tmp = new NodeEntry(source->getFullName(), m->name, nodeIndex, -1, simTime());
        Vec_BeaconsO.push_back(tmp);
    }
}


void Statistics::executeEachTimestep(bool simulationDone)
{
    if(collectMAClayerData)
        MAClayerToFile();

    if(collectVehiclesData)
    {
        vehiclesData();   // collecting data from all vehicles in each timeStep
        if(ev.isGUI()) vehiclesDataToFile();  // write what we have collected so far
    }

    if(LaneCostsMode > 0)
        laneCostsData();

    if(collectInductionLoopData)
    {
        inductionLoops();    // collecting induction loop data in each timeStep
        if(ev.isGUI()) inductionLoopToFile();  // write what we have collected so far
    }

    if(collectPlnManagerData)
    {
        if(ev.isGUI()) plnManageToFile();  // write what we have collected so far
        if(ev.isGUI()) plnStatToFile();
    }

    if(printIncidentDetection)
        incidentDetectionToFile();

    // todo:
    if(simulationDone)
    {
        if(collectVehiclesData && !ev.isGUI())
            vehiclesDataToFile();

        if(collectInductionLoopData && !ev.isGUI())
            inductionLoopToFile();

        if(collectPlnManagerData && !ev.isGUI())
        {
            plnManageToFile();
            plnStatToFile();
        }

        // sort the vectors by node ID:
        // Vec_BeaconsP = SortByID(Vec_BeaconsP);
        // Vec_BeaconsO = SortByID(Vec_BeaconsO);
        // Vec_BeaconsDP = SortByID(Vec_BeaconsDP);
        // Vec_BeaconsDO = SortByID(Vec_BeaconsDO);

        postProcess();

        if(printBeaconsStatistics)
            printToFile();
    }
}


void Statistics::HistogramsToFile()
{
    ofstream outFile;
    string VENTOSfullDirectory = cSimulation::getActiveSimulation()->getEnvir()->getConfig()->getConfigEntry("network").getBaseDirectory();
    string SUMODirectory = simulation.getSystemModule()->par("SUMODirectory").stringValue();
    string fileName = VENTOSfullDirectory + SUMODirectory + "/edgeWeights.txt";
    outFile.open(fileName.c_str()); //Open the edgeWeights file

    for(map<string, Histogram>::iterator it = edgeHistograms.begin(); it != edgeHistograms.end(); it++) //For each histogram
    {
        if(it->first != "") //If it has a name (empty-ID histograms occur when vehicles update in an intersection)
        {
            Histogram* hist = &it->second;
            outFile << it->first << " " << hist->count << endl; //Write the edge ID and its number of data points
            for(map<int, int>::iterator it2 = hist->data.begin(); it2 != hist->data.end(); it2++)
            {
                outFile << it2->first << " " << it2->second << "  ";    //And then write each data point followed by the number of occurrences
            }
            outFile << endl;
        }
    }
}


void Statistics::parseHistogramFile()
{
    ifstream inFile;
    string VENTOSfullDirectory = cSimulation::getActiveSimulation()->getEnvir()->getConfig()->getConfigEntry("network").getBaseDirectory();
    string SUMODirectory = simulation.getSystemModule()->par("SUMODirectory").stringValue();
    string fileName = VENTOSfullDirectory + SUMODirectory + "/edgeWeights.txt";
    inFile.open(fileName.c_str());  //Open the edgeWeights file

    string edgeName;
    while(inFile >> edgeName)   //While there are more edges to read
    {
        Histogram* hist = &edgeHistograms[edgeName];//Get the histogram for the edge
        inFile >> hist->count;                      //And read in the number of data points
        int edgeCount = 0;
        while(edgeCount < hist->count)  //While we haven't read in all the data points
        {
            int val;
            int valCount;
            inFile >> val;      //Read in a value and how many times it occurs
            inFile >> valCount;
            hist->data[val] = valCount; //And write this to the histogram
            hist->average = (hist->average * edgeCount + val * valCount) / (edgeCount + valCount);  //Update the running average
            edgeCount += valCount;  //And mark we read this many more data points
        }
    }
    inFile.close();
}


void Statistics::vehiclesData()
{
    // get all lanes in the network
    list<string> myList = TraCI->commandGetLaneList();

    for(list<string>::iterator i = myList.begin(); i != myList.end(); ++i)
    {
        // get all vehicles on lane i
        list<string> myList2 = TraCI->commandGetLaneVehicleList( i->c_str() );

        for(list<string>::reverse_iterator k = myList2.rbegin(); k != myList2.rend(); ++k)
            saveVehicleData(k->c_str());
    }

    // increase index after writing data for all vehicles
    if (TraCI->commandGetVehicleCount() > 0)
        index++;
}


void Statistics::saveVehicleData(string vID)
{
    double timeStep = (simTime()-updateInterval).dbl();
    string vType = TraCI->commandGetVehicleTypeId(vID);
    string lane = TraCI->commandGetVehicleLaneId(vID);
    double pos = TraCI->commandGetVehicleLanePosition(vID);
    double speed = TraCI->commandGetVehicleSpeed(vID);
    double accel = TraCI->commandGetVehicleAccel(vID);
    int CFMode_Enum = TraCI->commandGetVehicleCFMode(vID);
    string CFMode;

    enum CFMODES {
        Mode_Undefined,
        Mode_NoData,
        Mode_DataLoss,
        Mode_SpeedControl,
        Mode_GapControl,
        Mode_EmergencyBrake,
        Mode_Stopped
    };

    switch(CFMode_Enum)
    {
    case Mode_Undefined:
        CFMode = "Undefined";
        break;
    case Mode_NoData:
        CFMode = "NoData";
        break;
    case Mode_DataLoss:
        CFMode = "DataLoss";
        break;
    case Mode_SpeedControl:
        CFMode = "SpeedControl";
        break;
    case Mode_GapControl:
        CFMode = "GapControl";
        break;
    case Mode_EmergencyBrake:
        CFMode = "EmergencyBrake";
        break;
    case Mode_Stopped:
        CFMode = "Stopped";
        break;
    default:
        error("Not a valid CFModel!");
        break;
    }

    // get the timeGap setting
    double timeGapSetting = TraCI->commandGetVehicleTimeGap(vID);

    // get the gap
    vector<string> vleaderIDnew = TraCI->commandGetLeadingVehicle(vID, 900);
    string vleaderID = vleaderIDnew[0];
    double spaceGap = -1;

    if(vleaderID != "")
        spaceGap = atof( vleaderIDnew[1].c_str() );

    // calculate timeGap (if leading is present)
    double timeGap = -1;

    if(vleaderID != "" && speed != 0)
        timeGap = spaceGap / speed;

    VehicleData *tmp = new VehicleData(index, timeStep,
                                       vID.c_str(), vType.c_str(),
                                       lane.c_str(), pos,
                                       speed, accel, CFMode.c_str(),
                                       timeGapSetting, spaceGap, timeGap);
    Vec_vehiclesData.push_back(tmp);
}


void Statistics::vehiclesDataToFile()
{
    boost::filesystem::path filePath;

    if( ev.isGUI() )
    {
        filePath = "results/gui/vehicleData.txt";
    }
    else
    {
        // get the current run number
        int currentRun = ev.getConfigEx()->getActiveRunNumber();
        ostringstream fileName;
        fileName << currentRun << "_vehicleData.txt";
        filePath = "results/cmd/" + fileName.str();
    }

    FILE *filePtr = fopen (filePath.string().c_str(), "w");

    // write header
    fprintf (filePtr, "%-10s","index");
    fprintf (filePtr, "%-12s","timeStep");
    fprintf (filePtr, "%-15s","vehicleName");
    fprintf (filePtr, "%-17s","vehicleType");
    fprintf (filePtr, "%-12s","lane");
    fprintf (filePtr, "%-11s","pos");
    fprintf (filePtr, "%-12s","speed");
    fprintf (filePtr, "%-12s","accel");
    fprintf (filePtr, "%-20s","CFMode");
    fprintf (filePtr, "%-20s","timeGapSetting");
    fprintf (filePtr, "%-10s","SpaceGap");
    fprintf (filePtr, "%-10s\n\n","timeGap");

    int oldIndex = -1;

    // write body
    for(unsigned int k=0; k<Vec_vehiclesData.size(); k++)
    {
        if(oldIndex != Vec_vehiclesData[k]->index)
        {
            fprintf(filePtr, "\n");
            oldIndex = Vec_vehiclesData[k]->index;
        }

        fprintf (filePtr, "%-10d ", Vec_vehiclesData[k]->index);
        fprintf (filePtr, "%-10.2f ", Vec_vehiclesData[k]->time );
        fprintf (filePtr, "%-15s ", Vec_vehiclesData[k]->vehicleName);
        fprintf (filePtr, "%-15s ", Vec_vehiclesData[k]->vehicleType);
        fprintf (filePtr, "%-12s ", Vec_vehiclesData[k]->lane);
        fprintf (filePtr, "%-10.2f ", Vec_vehiclesData[k]->pos);
        fprintf (filePtr, "%-10.2f ", Vec_vehiclesData[k]->speed);
        fprintf (filePtr, "%-10.2f ", Vec_vehiclesData[k]->accel);
        fprintf (filePtr, "%-20s", Vec_vehiclesData[k]->CFMode);
        fprintf (filePtr, "%-20.2f ", Vec_vehiclesData[k]->timeGapSetting);
        fprintf (filePtr, "%-10.2f ", Vec_vehiclesData[k]->spaceGap);
        fprintf (filePtr, "%-10.2f \n", Vec_vehiclesData[k]->timeGap);
    }

    fclose(filePtr);
}

void Statistics::laneCostsData()
{
    cModule *module = simulation.getSystemModule()->getSubmodule("router");
    Router *r = static_cast< Router* >(module);

    list<string> vList = TraCI->commandGetVehicleList();

    for(list<string>::iterator it = vList.begin(); it != vList.end(); it++) //Look at each vehicle
    {
        string curEdge = TraCI->commandGetVehicleEdgeId(*it);  //The edge it's currently on
        if(TraCI->commandGetVehicleLanePosition(*it) * 1.05 > TraCI->commandGetLaneLength(TraCI->commandGetVehicleLaneId(*it)))   //If the vehicle is on (or extremely close to) the end of the lane
            curEdge = "";
        string prevEdge = vehicleEdges[*it];    //The last edge we saw it on
        if(vehicleEdges.find(*it) == vehicleEdges.end())    //If we haven't yet seen this vehicle
        {
            vehicleEdges[*it] = curEdge;           //Initialize its current edge
            vehicleTimes[*it] = simTime().dbl();   //And current time
            vehicleLaneChangeCount[*it] = -1;
        }

        if(prevEdge != curEdge) //If we've moved edges
        {
            if(r->UseHysteresis and ++vehicleLaneChangeCount[*it] > HysteresisCount * 2)
            {
                vehicleLaneChangeCount[*it] = 0;
                r->sendRerouteSignal(*it);
            }
            edgeHistograms[prevEdge].insert(simTime().dbl() - vehicleTimes[*it], LaneCostsMode);   //Add the time the vehicle traveled to the data set for that edge
            vehicleEdges[*it] = curEdge;                                            //And set its edge to the new one
            vehicleTimes[*it] = simTime().dbl();                                    //And that edges start time to now
            //cout << *it << " moves to edge " << curEdge << " at time " << simTime().dbl() << endl;  //Print a change
        }
    }
}


void Statistics::inductionLoops()
{
    // get all loop detectors
    list<string> str = TraCI->commandGetLoopDetectorList();

    // for each loop detector
    for (list<string>::iterator it=str.begin(); it != str.end(); ++it)
    {
        // only if this loop detector detected a vehicle
        if( TraCI->commandGetLoopDetectorCount(*it) == 1 )
        {
            vector<string>  st = TraCI->commandGetLoopDetectorVehicleData(*it);

            string vehicleName = st.at(0);
            double entryT = atof( st.at(2).c_str() );
            double leaveT = atof( st.at(3).c_str() );
            double speed = TraCI->commandGetLoopDetectorSpeed(*it);  // vehicle speed at current moment

            int counter = findInVector(Vec_loopDetectors, (*it).c_str(), vehicleName.c_str());

            // its a new entry, so we add it
            if(counter == -1)
            {
                LoopDetector *tmp = new LoopDetector( (*it).c_str(), vehicleName.c_str(), entryT, -1, speed, -1 );
                Vec_loopDetectors.push_back(tmp);
            }
            // if found, just update the leave time and leave speed
            else
            {
                Vec_loopDetectors[counter]->leaveTime = leaveT;
                Vec_loopDetectors[counter]->leaveSpeed = speed;
            }
        }
    }
}


void Statistics::inductionLoopToFile()
{
    boost::filesystem::path filePath;

    if( ev.isGUI() )
    {
        filePath = "results/gui/loopDetector.txt";
    }
    else
    {
        // get the current run number
        int currentRun = ev.getConfigEx()->getActiveRunNumber();
        ostringstream fileName;
        fileName << currentRun << "_loopDetector.txt";
        filePath = "results/cmd/" + fileName.str();
    }

    FILE *filePtr = fopen (filePath.string().c_str(), "w");

    // write header
    fprintf (filePtr, "%-20s","loopDetector");
    fprintf (filePtr, "%-20s","vehicleName");
    fprintf (filePtr, "%-20s","vehicleEntryTime");
    fprintf (filePtr, "%-20s","vehicleLeaveTime");
    fprintf (filePtr, "%-22s","vehicleEntrySpeed");
    fprintf (filePtr, "%-22s\n\n","vehicleLeaveSpeed");

    // write body
    for(unsigned int k=0; k<Vec_loopDetectors.size(); k++)
    {
        fprintf (filePtr, "%-20s ", Vec_loopDetectors[k]->detectorName);
        fprintf (filePtr, "%-20s ", Vec_loopDetectors[k]->vehicleName);
        fprintf (filePtr, "%-20.2f ", Vec_loopDetectors[k]->entryTime);
        fprintf (filePtr, "%-20.2f ", Vec_loopDetectors[k]->leaveTime);
        fprintf (filePtr, "%-20.2f ", Vec_loopDetectors[k]->entrySpeed);
        fprintf (filePtr, "%-20.2f ", Vec_loopDetectors[k]->leaveSpeed);
        fprintf (filePtr, "\n" );
    }

    fclose(filePtr);
}


void Statistics::incidentDetectionToFile()
{
    // get a pointer to any RSU
    cModule *module = simulation.getSystemModule()->getSubmodule("RSU", 0);

    if(module == NULL)
        return;

    ApplRSUManager *RSUptr = static_cast<ApplRSUManager *>(module);

    if(RSUptr == NULL)
        return;

    boost::filesystem::path filePath;

    if( ev.isGUI() )
    {
        filePath = "results/gui/IncidentTable.txt";
    }
    else
    {
        // get the current run number
        int currentRun = ev.getConfigEx()->getActiveRunNumber();
        ostringstream fileName;
        fileName << currentRun << "_IncidentTable.txt";
        filePath = "results/cmd/" + fileName.str();
    }

    ofstream filePtr( filePath.string().c_str() );

    if (filePtr.is_open())
    {
        filePtr << RSUptr->tableCount;
    }

    filePtr.close();
}


void Statistics::MAClayerToFile()
{
    boost::filesystem::path filePath;

    if( ev.isGUI() )
    {
        filePath = "results/gui/MACdata.txt";
    }
    else
    {
        // get the current run number
        int currentRun = ev.getConfigEx()->getActiveRunNumber();
        ostringstream fileName;
        fileName << currentRun << "_MACdata.txt";
        filePath = "results/cmd/" + fileName.str();
    }

    FILE *filePtr = fopen (filePath.string().c_str(), "w");

    // write header
    fprintf (filePtr, "%-20s","timeStep");
    fprintf (filePtr, "%-20s","vehicleName");

    fprintf (filePtr, "%-20s","DroppedPackets");
    fprintf (filePtr, "%-20s","NumTooLittleTime");
    fprintf (filePtr, "%-30s","NumInternalContention");
    fprintf (filePtr, "%-20s","NumBackoff");
    fprintf (filePtr, "%-20s","SlotsBackoff");
    fprintf (filePtr, "%-20s","TotalBusyTime");
    fprintf (filePtr, "%-20s","SentPackets");
    fprintf (filePtr, "%-20s","SNIRLostPackets");
    fprintf (filePtr, "%-20s","TXRXLostPackets");
    fprintf (filePtr, "%-20s","ReceivedPackets");
    fprintf (filePtr, "%-20s","ReceivedBroadcasts\n\n");

    // write body
    for(unsigned int k=0; k<Vec_MacStat.size(); k++)
    {
        fprintf (filePtr, "%-20.2f ", Vec_MacStat[k]->time);
        fprintf (filePtr, "%-20s ", Vec_MacStat[k]->name);

        fprintf (filePtr, "%-20ld ", Vec_MacStat[k]->MacStatsVec[0]);
        fprintf (filePtr, "%-20ld ", Vec_MacStat[k]->MacStatsVec[1]);
        fprintf (filePtr, "%-30ld ", Vec_MacStat[k]->MacStatsVec[2]);
        fprintf (filePtr, "%-20ld ", Vec_MacStat[k]->MacStatsVec[3]);
        fprintf (filePtr, "%-20ld ", Vec_MacStat[k]->MacStatsVec[4]);
        fprintf (filePtr, "%-20ld ", Vec_MacStat[k]->MacStatsVec[5]);
        fprintf (filePtr, "%-20ld ", Vec_MacStat[k]->MacStatsVec[6]);
        fprintf (filePtr, "%-20ld ", Vec_MacStat[k]->MacStatsVec[7]);
        fprintf (filePtr, "%-20ld ", Vec_MacStat[k]->MacStatsVec[8]);
        fprintf (filePtr, "%-20ld ", Vec_MacStat[k]->MacStatsVec[9]);
        fprintf (filePtr, "%-20ld ", Vec_MacStat[k]->MacStatsVec[10]);

        fprintf (filePtr, "\n" );
    }

    fclose(filePtr);
}


void Statistics::plnManageToFile()
{
    boost::filesystem::path filePath;

    if( ev.isGUI() )
    {
        filePath = "results/gui/plnManage.txt";
    }
    else
    {
        // get the current run number
        int currentRun = ev.getConfigEx()->getActiveRunNumber();
        ostringstream fileName;
        fileName << currentRun << "_plnManage.txt";
        filePath = "results/cmd/" + fileName.str();
    }

    FILE *filePtr = fopen (filePath.string().c_str(), "w");

    // write header
    fprintf (filePtr, "%-12s","timeStep");
    fprintf (filePtr, "%-15s","sender");
    fprintf (filePtr, "%-17s","receiver");
    fprintf (filePtr, "%-25s","type");
    fprintf (filePtr, "%-20s","sendingPlnID");
    fprintf (filePtr, "%-20s\n\n","recPlnID");

    string oldSender = "";
    double oldTime = -1;

    // write body
    for(unsigned int k=0; k<Vec_plnManagement.size(); k++)
    {
        // make the log more readable :)
        if(string(Vec_plnManagement[k]->sender) != oldSender || Vec_plnManagement[k]->time != oldTime)
        {
            fprintf(filePtr, "\n");
            oldSender = Vec_plnManagement[k]->sender;
            oldTime = Vec_plnManagement[k]->time;
        }

        fprintf (filePtr, "%-10.2f ", Vec_plnManagement[k]->time);
        fprintf (filePtr, "%-15s ", Vec_plnManagement[k]->sender);
        fprintf (filePtr, "%-17s ", Vec_plnManagement[k]->receiver);
        fprintf (filePtr, "%-30s ", Vec_plnManagement[k]->type);
        fprintf (filePtr, "%-18s ", Vec_plnManagement[k]->sendingPlnID);
        fprintf (filePtr, "%-20s\n", Vec_plnManagement[k]->receivingPlnID);
    }

    fclose(filePtr);
}


void Statistics::plnStatToFile()
{
    boost::filesystem::path filePath;

    if( ev.isGUI() )
    {
        filePath = "results/gui/plnStat.txt";
    }
    else
    {
        // get the current run number
        int currentRun = ev.getConfigEx()->getActiveRunNumber();
        ostringstream fileName;
        fileName << currentRun << "_plnStat.txt";
        filePath = "results/cmd/" + fileName.str();
    }

    FILE *filePtr = fopen (filePath.string().c_str(), "w");

    // write header
    fprintf (filePtr, "%-12s","timeStep");
    fprintf (filePtr, "%-20s","from platoon");
    fprintf (filePtr, "%-20s","to platoon");
    fprintf (filePtr, "%-20s\n\n","comment");

    string oldPln = "";

    // write body
    for(unsigned int k=0; k<Vec_plnStat.size(); k++)
    {
        if(Vec_plnStat[k]->from != oldPln)
        {
            fprintf(filePtr, "\n");
            oldPln = Vec_plnStat[k]->from;
        }

        fprintf (filePtr, "%-10.2f ", Vec_plnStat[k]->time);
        fprintf (filePtr, "%-20s ", Vec_plnStat[k]->from);
        fprintf (filePtr, "%-20s ", Vec_plnStat[k]->to);
        fprintf (filePtr, "%-20s\n", Vec_plnStat[k]->maneuver);
    }

    fclose(filePtr);
}


// todo
void Statistics::postProcess()
{
    for(unsigned int k=0; k<Vec_BeaconsP.size(); k++)
    {
        int counter = findInVector(totalBeaconsP, Vec_BeaconsP[k]->name1);

        // its a new entry, so we add it.
        if(counter == -1)
        {
            NodeEntry *tmp = new NodeEntry(Vec_BeaconsP[k]->name1, "-", Vec_BeaconsP[k]->nodeID, 1, -1);
            totalBeaconsP.push_back(tmp);
        }
        // if found, just update the existing fields
        else
        {
            totalBeaconsP[counter]->count = totalBeaconsP[counter]->count + 1;
        }
    }

    for(unsigned int k=0; k<Vec_BeaconsO.size(); k++)
    {
        int counter = findInVector(totalBeaconsO, Vec_BeaconsO[k]->name1);

        // its a new entry, so we add it.
        if(counter == -1)
        {
            NodeEntry *tmp = new NodeEntry(Vec_BeaconsO[k]->name1, "-", Vec_BeaconsO[k]->nodeID, 1, -1);
            totalBeaconsO.push_back(tmp);
        }
        // if found, just update the existing fields
        else
        {
            totalBeaconsO[counter]->count = totalBeaconsO[counter]->count + 1;
        }
    }

    for(unsigned int k=0; k<Vec_BeaconsDP.size(); k++)
    {
        int counter = findInVector(totalBeaconsDP, Vec_BeaconsDP[k]->name1);

        // its a new entry, so we add it.
        if(counter == -1)
        {
            NodeEntry *tmp = new NodeEntry(Vec_BeaconsDP[k]->name1, "-", Vec_BeaconsDP[k]->nodeID, 1, -1);
            totalBeaconsDP.push_back(tmp);
        }
        // if found, just update the existing fields
        else
        {
            totalBeaconsDP[counter]->count = totalBeaconsDP[counter]->count + 1;
        }
    }

    for(unsigned int k=0; k<Vec_BeaconsDO.size(); k++)
    {
        int counter = findInVector(totalBeaconsDO, Vec_BeaconsDO[k]->name1);

        // its a new entry, so we add it.
        if(counter == -1)
        {
            NodeEntry *tmp = new NodeEntry(Vec_BeaconsDO[k]->name1, "-", Vec_BeaconsDO[k]->nodeID, 1, -1);
            totalBeaconsDO.push_back(tmp);
        }
        // if found, just update the existing fields
        else
        {
            totalBeaconsDO[counter]->count = totalBeaconsDO[counter]->count + 1;
        }
    }

    double intervalS = 0;
    double intervalE = updateInterval;
    int val = 0;

    for(int i=1; i<(terminate/updateInterval); i++)
    {
        // it should not be sorted
        for(unsigned int k=0; k<Vec_BeaconsDO.size(); k++)
        {
            if(Vec_BeaconsDO[k]->time.dbl() >= intervalS && Vec_BeaconsDO[k]->time.dbl() <= intervalE)
                val++;
        }

        NodeEntry *tmp = new NodeEntry("-", "-", -1, val, intervalE);
        beaconsDO_interval.push_back(tmp);

        val = 0;
        intervalS = intervalS + updateInterval;
        intervalE = intervalE + updateInterval;
    }

    intervalS = 0;
    intervalE = updateInterval;
    val = 0;

    for(int i=1; i<(terminate/updateInterval); i++)
    {
        // it should not be sorted
        for(unsigned int k=0; k<Vec_BeaconsDP.size(); k++)
        {
            if(Vec_BeaconsDP[k]->time.dbl() >= intervalS && Vec_BeaconsDP[k]->time.dbl() <= intervalE)
                val++;
        }

        NodeEntry *tmp = new NodeEntry("-", "-", -1, val, intervalE);
        beaconsDP_interval.push_back(tmp);

        val = 0;
        intervalS = intervalS + updateInterval;
        intervalE = intervalE + updateInterval;
    }
}


// todo
void Statistics::printToFile()
{
    char fName [50];
    FILE *f1;

    if( ev.isGUI() )
    {
        sprintf (fName, "%s.txt", "results/gui/01.beacons_total_p");
    }
    else
    {
        // get the current run number
        int currentRun = ev.getConfigEx()->getActiveRunNumber();
        sprintf (fName, "%s_%d.txt", "results/cmd/01.beacons_total_p", currentRun);
    }

    f1 = fopen (fName, "w");

    fprintf (f1, "%s\n\n","Total Number of received beacons from preceding vehicle");

    // write header
    fprintf (f1, "%-10s","vehicle");
    fprintf (f1, "%-10s\n","beacons");  // beacon from preceding

    for(unsigned int k=0; k<totalBeaconsP.size(); k++)
    {
        fprintf (f1, "%-10s ", totalBeaconsP[k]->name1);
        fprintf (f1, "%-10d ", totalBeaconsP[k]->count);
        fprintf (f1, "\n");
    }

    fclose(f1);

    // ##########################################################
    // ##########################################################

    if( ev.isGUI() )
    {
        sprintf (fName, "%s.txt", "results/gui/02.beacons_total_o");
    }
    else
    {
        // get the current run number
        int currentRun = ev.getConfigEx()->getActiveRunNumber();
        sprintf (fName, "%s_%d.txt", "results/cmd/02.beacons_total_o", currentRun);
    }

    f1 = fopen (fName, "w");

    fprintf (f1, "%s\n\n","Total Number of received beacons from other vehicle");

    // write header
    fprintf (f1, "%-10s","vehicle");
    fprintf (f1, "%-10s\n","beacons");  // beacon from preceding

    for(unsigned int k=0; k<totalBeaconsO.size(); k++)
    {
        fprintf (f1, "%-10s ", totalBeaconsO[k]->name1);
        fprintf (f1, "%-10d ", totalBeaconsO[k]->count);
        fprintf (f1, "\n");
    }

    fclose(f1);

    // ##########################################################
    // ##########################################################

    if( ev.isGUI() )
    {
        sprintf (fName, "%s.txt", "results/gui/03.beacons_total_droped_p");
    }
    else
    {
        // get the current run number
        int currentRun = ev.getConfigEx()->getActiveRunNumber();
        sprintf (fName, "%s_%d.txt", "results/cmd/03.beacons_total_droped_p", currentRun);
    }

    f1 = fopen (fName, "w");

    fprintf (f1, "%s\n\n","Total Number of dropped beacons from preceding vehicle");

    // write header
    fprintf (f1, "%-10s","vehicle");
    fprintf (f1, "%-10s\n","beacons");  // beacon from preceding

    for(unsigned int k=0; k<totalBeaconsDP.size(); k++)
    {
        fprintf (f1, "%-10s ", totalBeaconsDP[k]->name1);
        fprintf (f1, "%-10d ", totalBeaconsDP[k]->count);
        fprintf (f1, "\n");
    }

    fclose(f1);

    // ##########################################################
    // ##########################################################

    if( ev.isGUI() )
    {
        sprintf (fName, "%s.txt", "results/gui/04.beacons_total_droped_o");
    }
    else
    {
        // get the current run number
        int currentRun = ev.getConfigEx()->getActiveRunNumber();
        sprintf (fName, "%s_%d.txt", "results/cmd/04.beacons_total_droped_o", currentRun);
    }

    f1 = fopen (fName, "w");

    fprintf (f1, "%s\n\n","Total Number of dropped beacons from other vehicles");

    // write header
    fprintf (f1, "%-10s","vehicle");
    fprintf (f1, "%-10s\n","beacons");  // beacon from preceding

    for(unsigned int k=0; k<totalBeaconsDO.size(); k++)
    {
        fprintf (f1, "%-10s ", totalBeaconsDO[k]->name1);
        fprintf (f1, "%-10d ", totalBeaconsDO[k]->count);
        fprintf (f1, "\n");
    }

    fclose(f1);

    // ##########################################################
    // ##########################################################

    if( ev.isGUI() )
    {
        sprintf (fName, "%s.txt", "results/gui/05.beacon_interval_droped_o");
    }
    else
    {
        // get the current run number
        int currentRun = ev.getConfigEx()->getActiveRunNumber();
        sprintf (fName, "%s_%d.txt", "results/cmd/05.beacon_interval_droped_o", currentRun);
    }

    f1 = fopen (fName, "w");

    fprintf (f1, "%s\n\n","Number of dropped beacons (from other vehicles) in each interval");

    // write header
    fprintf (f1, "%-10s","vehicle");
    fprintf (f1, "%-10s","delta");
    fprintf (f1, "%-10s\n","beacons");  // beacon from preceding

    for(unsigned int k=0; k<beaconsDO_interval.size(); k++)
    {
        fprintf (f1, "%-10s ", beaconsDO_interval[k]->name1);
        fprintf (f1, "%-10f ", beaconsDO_interval[k]->time.dbl());
        fprintf (f1, "%-10d ", beaconsDO_interval[k]->count);
        fprintf (f1, "\n");
    }

    fclose(f1);

    // ##########################################################
    // ##########################################################

    if( ev.isGUI() )
    {
        sprintf (fName, "%s.txt", "results/gui/06.beacon_interval_droped_p");
    }
    else
    {
        // get the current run number
        int currentRun = ev.getConfigEx()->getActiveRunNumber();
        sprintf (fName, "%s_%d.txt", "results/cmd/06.beacon_interval_droped_p", currentRun);
    }

    f1 = fopen (fName, "w");

    fprintf (f1, "%s\n\n","Number of dropped beacons (from proceeding vehicle) in each interval");

    // write header
    fprintf (f1, "%-10s","vehicle");
    fprintf (f1, "%-10s","delta");
    fprintf (f1, "%-10s\n","beacons");  // beacon from preceding

    for(unsigned int k=0; k<beaconsDP_interval.size(); k++)
    {
        fprintf (f1, "%-10s ", beaconsDP_interval[k]->name1);
        fprintf (f1, "%-10f ", beaconsDP_interval[k]->time.dbl());
        fprintf (f1, "%-10d ", beaconsDP_interval[k]->count);
        fprintf (f1, "\n");
    }

    fclose(f1);
}


// returns the index of a node. for example gets V[10] as input and returns 10
int Statistics::getNodeIndex(const char *ModName)
{
    ostringstream oss;

    for(int h=0 ; ModName[h] != NULL ; h++)
    {
        if ( isdigit(ModName[h]) )
        {
            oss << ModName[h];
        }
    }

    int nodeID = atoi(oss.str().c_str());

    return nodeID;
}


// todo: use template
int Statistics::findInVector(vector<NodeEntry *> Vec, const char *name)
{
    unsigned int counter;    // for counter
    bool found = false;

    for(counter=0; counter<Vec.size(); counter++)
    {
        if( strcmp(Vec[counter]->name1, name) == 0 )
        {
            found = true;
            break;
        }
    }

    if(!found)
        return -1;
    else
        return counter;
}


int Statistics::findInVector(vector<MacStatEntry *> Vec, const char *name)
{
    unsigned int counter;    // for counter
    bool found = false;

    for(counter=0; counter<Vec.size(); counter++)
    {
        if( strcmp(Vec[counter]->name, name) == 0 )
        {
            found = true;
            break;
        }
    }

    if(!found)
        return -1;
    else
        return counter;
}


int Statistics::findInVector(vector<LoopDetector *> Vec, const char *detectorName, const char *vehicleName)
{
    unsigned int counter;
    bool found = false;

    for(counter = 0; counter < Vec.size(); counter++)
    {
        if( strcmp(Vec[counter]->detectorName, detectorName) == 0 && strcmp(Vec[counter]->vehicleName, vehicleName) == 0)
        {
            found = true;
            break;
        }
    }

    if(!found)
        return -1;
    else
        return counter;
}


vector<NodeEntry *> Statistics::SortByID(vector<NodeEntry *> vec)
{
    if(vec.size() == 0)
        return vec;

    for(unsigned int i = 0; i<vec.size()-1; i++)
    {
        for(unsigned int j = i+1; j<vec.size(); j++)
        {
            if( vec[i]->nodeID > vec[j]->nodeID)
                swap(vec[i], vec[j]);
        }
    }

    return vec;
}


}
