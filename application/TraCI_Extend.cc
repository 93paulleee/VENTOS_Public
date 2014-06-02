
#include "TraCI_Extend.h"
#include <sstream>
#include <fstream>
#include <iostream>

Define_Module(TraCI_Extend);


TraCI_Extend::~TraCI_Extend()
{

}


void TraCI_Extend::initialize(int stage)
{
    TraCIScenarioManager::initialize(stage);

    if (stage == 1)
    {
        launchConfig = par("launchConfig").xmlValue();
        seed = par("seed");
        SUMOfilesDir = par("SUMOfilesDir").stringValue();

        // example: sumo/CACC_Platoon
        if(SUMOfilesDir == "")
        {
            error("SUMO files directory is not specified!");
        }

        // example: /home/mani/Desktop/VENTOS
        VENTOSdirectory = cSimulation::getActiveSimulation()->getEnvir()->getConfig()->getConfigEntry("network").getBaseDirectory();

        // example: /home/mani/Desktop/VENTOS/sumo/CACC_Platoon
        SUMOfullDirectory = VENTOSdirectory + SUMOfilesDir;

        cXMLElement* basedir_node = new cXMLElement("basedir", __FILE__, launchConfig);
        basedir_node->setAttribute("path", SUMOfullDirectory.c_str());
        launchConfig->appendChild(basedir_node);

        // getting the seed
        cXMLElementList seed_nodes = launchConfig->getElementsByTagName("seed");
        if (seed_nodes.size() == 0)
        {
            if (seed == -1)
            {
                // default seed is current repetition
                const char* seed_s = cSimulation::getActiveSimulation()->getEnvir()->getConfigEx()->getVariable(CFGVAR_RUNNUMBER);
                seed = atoi(seed_s);
            }

            std::stringstream ss; ss << seed;
            cXMLElement* seed_node = new cXMLElement("seed", __FILE__, launchConfig);
            seed_node->setAttribute("value", ss.str().c_str());
            launchConfig->appendChild(seed_node);
        }
    }
    else if(stage == 2)
    {
        // todo: here we can put the equivalent python
        // code to create the server process
    }
}


void TraCI_Extend::handleSelfMsg(cMessage *msg)
{
    TraCIScenarioManager::handleSelfMsg(msg);
}


void TraCI_Extend::init_traci()
{
    // get the version of Python launchd
    std::pair<uint32_t, std::string> version = commandGetVersion();
    uint32_t apiVersion = version.first;
    std::string serverVersion = version.second;
    EV << "TraCI launchd reports apiVersion: " << apiVersion << " and serverVersion: " << serverVersion << endl;
    ASSERT(apiVersion == 1);

    // send the launchConfig to SUMO
    std::string contents = launchConfig->tostr(0);
    commandSendFile(contents);

    // call commandGetVersion again to
    // get the version of SUMO TraCI server
    std::pair<uint32_t, std::string> version2 = commandGetVersion();
    uint32_t apiVersion2 = version2.first;
    std::string serverVersion2 = version2.second;
    EV << "TraCI server reports apiVersion: " << apiVersion2 << " and serverVersion: " << serverVersion2 << endl;
    if ( !(apiVersion2 == 3 || apiVersion2 == 5 || apiVersion2 == 6 || apiVersion2 == 7 || apiVersion2 == 8) )
        error("TraCI server is unsupported.");

    // get network boundaries from SUMO
    double* bounds = commandGetNetworkBoundary();
    EV << "TraCI reports network boundaries (" << bounds[0] << ", " << bounds[1] << ")-(" << bounds[2] << ", " << bounds[3] << ")" << endl;

    // change to OMNET++ coordinates
    netbounds1 = TraCICoord(bounds[0], bounds[1]);
    netbounds2 = TraCICoord(bounds[2], bounds[3]);
    if ((traci2omnet(netbounds2).x > world->getPgs()->x) || (traci2omnet(netbounds1).y > world->getPgs()->y))
        EV << "WARNING: Playground size (" << world->getPgs()->x << ", " << world->getPgs()->y << ") might be too small for vehicle at network bounds (" << traci2omnet(netbounds2).x << ", " << traci2omnet(netbounds1).y << ")" << endl;

    // subscribe to list of departed and arrived vehicles, as well as simulation time
    commandSubscribeSimulation();

    // subscribe to list of vehicle ids
    commandSubscribeVehicle();

    ObstacleControl* obstacles = ObstacleControlAccess().getIfExists();

    if (obstacles)
    {
        // get list of polygons
        std::list<std::string> ids = commandGetPolygonIds();

        for (std::list<std::string>::iterator i = ids.begin(); i != ids.end(); ++i)
        {
            std::string id = *i;
            std::string typeId = commandGetPolygonTypeId(id);

            if (typeId == "building")
            {
                std::list<Coord> coords = commandGetPolygonShape(id);
                Obstacle obs(id, 9, .4); // each building gets attenuation of 9 dB per wall, 0.4 dB per meter
                std::vector<Coord> shape;
                std::copy(coords.begin(), coords.end(), std::back_inserter(shape));
                obs.setShape(shape);
                obstacles->add(obs);
            }
        }
    }
}


// #########################
// CMD_GET_VEHICLE_VARIABLE
// #########################

uint32_t TraCI_Extend::commandGetNoVehicles()
{
    return genericGetInt32(CMD_GET_VEHICLE_VARIABLE, "", ID_COUNT, RESPONSE_GET_VEHICLE_VARIABLE);
}


// gets a list of all vehicles in the network (alphabetically!!!)
std::list<std::string> TraCI_Extend::commandGetVehicleList()
{
    return genericGetStringList(CMD_GET_VEHICLE_VARIABLE, "", ID_LIST, RESPONSE_GET_VEHICLE_VARIABLE);
}


double TraCI_Extend::commandGetVehicleSpeed(std::string nodeId)
{
    return genericGetDouble(CMD_GET_VEHICLE_VARIABLE, nodeId, VAR_SPEED, RESPONSE_GET_VEHICLE_VARIABLE);
}


double TraCI_Extend::commandGetVehicleAccel(std::string nodeId)
{
    return genericGetDouble(CMD_GET_VEHICLE_VARIABLE, nodeId, 0x41, RESPONSE_GET_VEHICLE_VARIABLE);
}


std::string TraCI_Extend::commandGetVehicleType(std::string nodeId)
{
    return genericGetString(CMD_GET_VEHICLE_VARIABLE, nodeId, VAR_TYPE, RESPONSE_GET_VEHICLE_VARIABLE);
}


double TraCI_Extend::commandGetVehicleLength(std::string nodeId)
{
    return genericGetDouble(CMD_GET_VEHICLE_VARIABLE, nodeId, VAR_LENGTH, RESPONSE_GET_VEHICLE_VARIABLE);
}


double TraCI_Extend::commandGetVehicleMinGap(std::string nodeId)
{
    return genericGetDouble(CMD_GET_VEHICLE_VARIABLE, nodeId, VAR_MINGAP, RESPONSE_GET_VEHICLE_VARIABLE);
}


double TraCI_Extend::commandGetVehicleMaxDecel(std::string nodeId)
{
    return genericGetDouble(CMD_GET_VEHICLE_VARIABLE, nodeId, VAR_DECEL, RESPONSE_GET_VEHICLE_VARIABLE);
}


Coord TraCI_Extend::commandGetVehiclePos(std::string nodeId)
{
    return genericGetCoordv2(CMD_GET_VEHICLE_VARIABLE, nodeId, VAR_POSITION, RESPONSE_GET_VEHICLE_VARIABLE);
}


uint32_t TraCI_Extend::commandGetLaneIndex(std::string nodeId)
{
    return genericGetInt32(CMD_GET_VEHICLE_VARIABLE, nodeId, VAR_LANE_INDEX, RESPONSE_GET_VEHICLE_VARIABLE);
}


std::vector<std::string> TraCI_Extend::commandGetLeading(std::string nodeId, double look_ahead_distance)
{
    uint8_t requestTypeId = TYPE_DOUBLE;
    uint8_t resultTypeId = TYPE_COMPOUND;
    uint8_t variableId = 0x68;
    uint8_t responseId = RESPONSE_GET_VEHICLE_VARIABLE;

    TraCIBuffer buf = queryTraCI(CMD_GET_VEHICLE_VARIABLE, TraCIBuffer() << variableId << nodeId << requestTypeId << look_ahead_distance);

    uint8_t cmdLength; buf >> cmdLength;
    if (cmdLength == 0) {
        uint32_t cmdLengthX;
        buf >> cmdLengthX;
    }
    uint8_t commandId_r; buf >> commandId_r;
    ASSERT(commandId_r == responseId);
    uint8_t varId; buf >> varId;
    ASSERT(varId == variableId);
    std::string objectId_r; buf >> objectId_r;
    ASSERT(objectId_r == nodeId);

    uint8_t resType_r; buf >> resType_r;
    ASSERT(resType_r == resultTypeId);
    uint32_t count; buf >> count;

    // now we start getting real data that we are looking for
    std::vector<std::string> res;

    uint8_t dType; buf >> dType;
    std::string id; buf >> id;
    res.push_back(id);

    uint8_t dType2; buf >> dType2;
    double len; buf >> len;

    // the distance does not include minGap
    // we will add minGap to the len
    len = len + commandGetVehicleMinGap(nodeId);

    // now convert len to string
    std::ostringstream s;
    s << len;
    res.push_back(s.str());

    ASSERT(buf.eof());

    return res;
}


// my own method! [deprecated!]
std::string TraCI_Extend::commandGetLeading_old(std::string nodeId)
{
    // get the lane id (like 1to2_0)
    std::string laneID = commandGetLaneId(nodeId);

    // get a list of all vehicles on this lane (left to right)
    std::list<std::string> list = commandGetVehiclesOnLane(laneID);

    std::string vleaderID = "";

    for(std::list<std::string>::reverse_iterator i = list.rbegin(); i != list.rend(); ++i)
    {
        std::string currentID = i->c_str();

        if(currentID == nodeId)
            return vleaderID;

        vleaderID = i->c_str();
    }

    return "";
}


uint8_t* TraCI_Extend::commandGetVehicleColor(std::string nodeId)
{
    return genericGetArrayUnsignedInt(CMD_GET_VEHICLE_VARIABLE, nodeId, VAR_COLOR, RESPONSE_GET_VEHICLE_VARIABLE);
}



// ###############################
// CMD_GET_INDUCTIONLOOP_VARIABLE
// ###############################

std::list<std::string> TraCI_Extend::commandGetLoopDetectorList()
{
    return genericGetStringList(CMD_GET_INDUCTIONLOOP_VARIABLE, "", 0x00, RESPONSE_GET_INDUCTIONLOOP_VARIABLE);
}


uint32_t TraCI_Extend::commandGetLoopDetectorCount(std::string loopId)
{
    return genericGetInt32(CMD_GET_INDUCTIONLOOP_VARIABLE, loopId, 0x10, RESPONSE_GET_INDUCTIONLOOP_VARIABLE);
}


double TraCI_Extend::commandGetLoopDetectorSpeed(std::string loopId)
{
    return genericGetDouble(CMD_GET_INDUCTIONLOOP_VARIABLE, loopId, 0x11, RESPONSE_GET_INDUCTIONLOOP_VARIABLE);
}


std::list<std::string> TraCI_Extend::commandGetLoopDetectorVehicleList(std::string loopId)
{
    return genericGetStringList(CMD_GET_INDUCTIONLOOP_VARIABLE, loopId, 0x12, RESPONSE_GET_INDUCTIONLOOP_VARIABLE);
}


std::vector<std::string> TraCI_Extend::commandGetLoopDetectorVehicleData(std::string loopId)
{
    uint8_t resultTypeId = TYPE_COMPOUND;   // note: type is compound!
    uint8_t variableId = 0x17;
    uint8_t responseId = RESPONSE_GET_INDUCTIONLOOP_VARIABLE;

    TraCIBuffer buf = queryTraCI(CMD_GET_INDUCTIONLOOP_VARIABLE, TraCIBuffer() << variableId << loopId);

    uint8_t cmdLength; buf >> cmdLength;
    if (cmdLength == 0) {
        uint32_t cmdLengthX;
        buf >> cmdLengthX;
    }
    uint8_t commandId_r; buf >> commandId_r;
    ASSERT(commandId_r == responseId);
    uint8_t varId; buf >> varId;
    ASSERT(varId == variableId);
    std::string objectId_r; buf >> objectId_r;
    ASSERT(objectId_r == loopId);

    uint8_t resType_r; buf >> resType_r;
    ASSERT(resType_r == resultTypeId);
    uint32_t count; buf >> count;

    // now we start getting real data that we are looking for
    std::vector<std::string> res;
    std::ostringstream strs;

    // number of information packets (in 'No' variable)
    uint8_t typeI; buf >> typeI;
    uint32_t No; buf >> No;

    for (uint32_t i = 1; i <= No; i++)
    {
        // get vehicle id
        uint8_t typeS1; buf >> typeS1;
        std::string vId; buf >> vId;
        res.push_back(vId);

        // get vehicle length
        uint8_t dType2; buf >> dType2;
        double len; buf >> len;
        strs.str("");
        strs << len;
        res.push_back( strs.str() );

        // entryTime
        uint8_t dType3; buf >> dType3;
        double entryTime; buf >> entryTime;
        strs.str("");
        strs << entryTime;
        res.push_back( strs.str() );

        // leaveTime
        uint8_t dType4; buf >> dType4;
        double leaveTime; buf >> leaveTime;
        strs.str("");
        strs << leaveTime;
        res.push_back( strs.str() );

        // vehicle type
        uint8_t dType5; buf >> dType5;
        std::string vehicleTypeID; buf >> vehicleTypeID;
        strs.str("");
        strs << vehicleTypeID;
        res.push_back( strs.str() );
    }

    ASSERT(buf.eof());

    return res;
}


// ############################
// CMD_GET_VEHICLETYPE_VARIABLE
// ############################

double TraCI_Extend::commandGetVehicleLength_Type(std::string nodeId)
{
    return genericGetDouble(CMD_GET_VEHICLETYPE_VARIABLE, nodeId, VAR_LENGTH, RESPONSE_GET_VEHICLETYPE_VARIABLE);
}


// #####################
// CMD_GET_LANE_VARIABLE
// #####################

// gets a list of all lanes in the network
std::list<std::string> TraCI_Extend::commandGetLaneList()
{
    return genericGetStringList(CMD_GET_LANE_VARIABLE, "", ID_LIST, RESPONSE_GET_LANE_VARIABLE);
}


// gets a list of all lanes in the network
std::list<std::string> TraCI_Extend::commandGetVehicleLaneList(std::string edgeId)
{
    return genericGetStringList(CMD_GET_LANE_VARIABLE, edgeId, LAST_STEP_VEHICLE_ID_LIST, RESPONSE_GET_LANE_VARIABLE);
}


std::list<std::string> TraCI_Extend::commandGetVehiclesOnLane(std::string laneId)
{
    return genericGetStringList(CMD_GET_LANE_VARIABLE, laneId, LAST_STEP_VEHICLE_ID_LIST, RESPONSE_GET_LANE_VARIABLE);
}


// ########################
// RSU
// ########################

std::deque<RSUEntry*> TraCI_Extend::commandReadRSUsCoord(std::string RSUfilePath)
{
    file<> xmlFile( RSUfilePath.c_str() );        // Convert our file to a rapid-xml readable object
    xml_document<> doc;                           // Build a rapidxml doc
    doc.parse<0>(xmlFile.data());                 // Fill it with data from our file
    xml_node<> *node = doc.first_node("RSUs");  // Parse up to the "shapes" declaration

    for(node = node->first_node("poly"); node; node = node->next_sibling()) // For each node in nodes
    {
        std::string RSUname = "";
        std::string RSUtype = "";
        std::string RSUcoordinates = "";
        int readCount = 1;

        // For each node, iterate over its attributes until we reach "shape"
        for(xml_attribute<> *attr = node->first_attribute(); attr; attr = attr->next_attribute())
        {
            if(readCount == 1)
            {
                RSUname = attr->value();
            }
            else if(readCount == 2)
            {
                RSUtype = attr->value();
            }
            else if(readCount == 3)
            {
                RSUcoordinates = attr->value();
            }

            readCount++;
        }

        // tokenize coordinates
        int readCount2 = 1;
        double x;
        double y;
        char_separator<char> sep(",");
        tokenizer< char_separator<char> > tokens(RSUcoordinates, sep);

        for(tokenizer< char_separator<char> >::iterator beg=tokens.begin(); beg!=tokens.end();++beg)
        {
            if(readCount2 == 1)
            {
                x = std::atof( (*beg).c_str() );
            }
            else if(readCount2 == 2)
            {
                y = std::atof( (*beg).c_str() );
            }

            readCount2++;
        }

        // add it into queue (with TraCI coordinates)
        RSUEntry *entry = new RSUEntry(RSUname.c_str(), x, y);
        RSUs.push_back(entry);
    }

    return RSUs;
}


Coord* TraCI_Extend::commandGetRSUsCoord(unsigned int index)
{
    if( RSUs.size() == 0 )
        error("No RSUs have been initialized!");

    if( index < 0 || index >= RSUs.size() )
        error("index out of bound!");

    Coord* point = new Coord(RSUs[index]->coordX, RSUs[index]->coordY);
    return point;
}


// ####################
// CMD_GET_SIM_VARIABLE
// ####################

double* TraCI_Extend::commandGetNetworkBoundary()
{
    // query road network boundaries
    TraCIBuffer buf = queryTraCI(CMD_GET_SIM_VARIABLE, TraCIBuffer() << static_cast<uint8_t>(VAR_NET_BOUNDING_BOX) << std::string("sim0"));

    uint8_t cmdLength_resp; buf >> cmdLength_resp;
    uint8_t commandId_resp; buf >> commandId_resp; ASSERT(commandId_resp == RESPONSE_GET_SIM_VARIABLE);
    uint8_t variableId_resp; buf >> variableId_resp; ASSERT(variableId_resp == VAR_NET_BOUNDING_BOX);
    std::string simId; buf >> simId;
    uint8_t typeId_resp; buf >> typeId_resp; ASSERT(typeId_resp == TYPE_BOUNDINGBOX);

    double *boundaries = new double[4] ;

    buf >> boundaries[0];
    buf >> boundaries[1];
    buf >> boundaries[2];
    buf >> boundaries[3];

    ASSERT(buf.eof());

    return boundaries;
}


// ########################
// Control-related commands
// ########################

void TraCI_Extend::commandTerminate()
{
        TraCIBuffer buf = queryTraCI(CMD_CLOSE, TraCIBuffer() << 0);

        uint32_t count;
        buf >> count;
}


void TraCI_Extend::commandSendFile(std::string contents)
{
    uint8_t commandId = 0x75;
    TraCIBuffer buf;
    buf << std::string("sumo-launchd.launch.xml") << contents;
    sendTraCIMessage(makeTraCICommand(commandId, buf));

    TraCIBuffer obuf(receiveTraCIMessage());
    uint8_t cmdLength; obuf >> cmdLength;
    uint8_t commandResp; obuf >> commandResp;
    if (commandResp != commandId)
    {
        error("Expected response to command %d, but got one for command %d", commandId, commandResp);
    }

    uint8_t result; obuf >> result;
    std::string description; obuf >> description;
    if (result != RTYPE_OK)
    {
        EV << "Warning: Received non-OK response from TraCI server to command " << commandId << ":" << description.c_str() << std::endl;
    }
}


// ######################
// subscription commands
// ######################

void TraCI_Extend::commandSubscribeSimulation()
{
    uint32_t beginTime = 0;
    uint32_t endTime = 0x7FFFFFFF;
    std::string objectId = "";
    uint8_t variableNumber = 5;
    uint8_t variable1 = VAR_DEPARTED_VEHICLES_IDS;
    uint8_t variable2 = VAR_ARRIVED_VEHICLES_IDS;
    uint8_t variable3 = VAR_TIME_STEP;
    uint8_t variable4 = VAR_TELEPORT_STARTING_VEHICLES_IDS;
    uint8_t variable5 = VAR_TELEPORT_ENDING_VEHICLES_IDS;

    TraCIBuffer buf = queryTraCI(CMD_SUBSCRIBE_SIM_VARIABLE, TraCIBuffer() << beginTime << endTime
                                                                                        << objectId
                                                                                        << variableNumber
                                                                                        << variable1
                                                                                        << variable2
                                                                                        << variable3
                                                                                        << variable4
                                                                                        << variable5
                                                                                        );
    processSubcriptionResult(buf);

    ASSERT(buf.eof());
}


void TraCI_Extend::commandSubscribeVehicle()
{
    uint32_t beginTime = 0;
    uint32_t endTime = 0x7FFFFFFF;
    std::string objectId = "";
    uint8_t variableNumber = 1;
    uint8_t variable1 = ID_LIST;

    TraCIBuffer buf = queryTraCI(CMD_SUBSCRIBE_VEHICLE_VARIABLE, TraCIBuffer() << beginTime << endTime
                                                                                            << objectId
                                                                                            << variableNumber
                                                                                            << variable1
                                                                                            );
    processSubcriptionResult(buf);

    ASSERT(buf.eof());
}


// ############################
// generic methods for getters
// ############################

uint32_t TraCI_Extend::genericGetInt32(uint8_t commandId, std::string objectId, uint8_t variableId, uint8_t responseId)
{
    uint8_t resultTypeId = TYPE_INTEGER;
    uint32_t res;

    TraCIBuffer buf = TraCIScenarioManager::queryTraCI(commandId, TraCIBuffer() << variableId << objectId);

    uint8_t cmdLength; buf >> cmdLength;
    if (cmdLength == 0) {
        uint32_t cmdLengthX;
        buf >> cmdLengthX;
    }
    uint8_t commandId_r; buf >> commandId_r;
    ASSERT(commandId_r == responseId);
    uint8_t varId; buf >> varId;
    ASSERT(varId == variableId);
    std::string objectId_r; buf >> objectId_r;
    ASSERT(objectId_r == objectId);
    uint8_t resType_r; buf >> resType_r;
    ASSERT(resType_r == resultTypeId);
    buf >> res;

    ASSERT(buf.eof());

    return res;
}


// same as genericGetCoordv, but no conversion to omnet++ coordinates at the end
Coord TraCI_Extend::genericGetCoordv2(uint8_t commandId, std::string objectId, uint8_t variableId, uint8_t responseId)
{
    uint8_t resultTypeId = POSITION_2D;
    double x;
    double y;

    TraCIBuffer buf = queryTraCI(commandId, TraCIBuffer() << variableId << objectId);

    uint8_t cmdLength; buf >> cmdLength;
    if (cmdLength == 0) {
        uint32_t cmdLengthX;
        buf >> cmdLengthX;
    }
    uint8_t commandId_r; buf >> commandId_r;
    ASSERT(commandId_r == responseId);
    uint8_t varId; buf >> varId;
    ASSERT(varId == variableId);
    std::string objectId_r; buf >> objectId_r;
    ASSERT(objectId_r == objectId);
    uint8_t resType_r; buf >> resType_r;
    ASSERT(resType_r == resultTypeId);

    // now we start getting real data that we are looking for
    buf >> x;
    buf >> y;

    ASSERT(buf.eof());

    return Coord(x, y);
}


uint8_t* TraCI_Extend::genericGetArrayUnsignedInt(uint8_t commandId, std::string objectId, uint8_t variableId, uint8_t responseId)
{
    uint8_t resultTypeId = TYPE_COLOR;
    uint8_t* color = new uint8_t[4]; // RGBA

    TraCIBuffer buf = queryTraCI(commandId, TraCIBuffer() << variableId << objectId);

    uint8_t cmdLength; buf >> cmdLength;
    if (cmdLength == 0) {
        uint32_t cmdLengthX;
        buf >> cmdLengthX;
    }
    uint8_t commandId_r; buf >> commandId_r;
    ASSERT(commandId_r == responseId);
    uint8_t varId; buf >> varId;
    ASSERT(varId == variableId);
    std::string objectId_r; buf >> objectId_r;
    ASSERT(objectId_r == objectId);
    uint8_t resType_r; buf >> resType_r;
    ASSERT(resType_r == resultTypeId);

    // now we start getting real data that we are looking for
    buf >> color[0];
    buf >> color[1];
    buf >> color[2];
    buf >> color[3];

    ASSERT(buf.eof());

    return color;
}


// #########################
// CMD_SET_VEHICLE_VARIABLE
// #########################

void TraCI_Extend::commandSetMaxAccel(std::string nodeId, double value)
{
    uint8_t variableId = VAR_ACCEL;
    uint8_t variableType = TYPE_DOUBLE;

    TraCIBuffer buf = queryTraCI(CMD_SET_VEHICLE_VARIABLE, TraCIBuffer() << variableId << nodeId << variableType << value);
    ASSERT(buf.eof());
}


void TraCI_Extend::commandSetMaxDecel(std::string nodeId, double value)
{
    uint8_t variableId = VAR_DECEL;
    uint8_t variableType = TYPE_DOUBLE;

    TraCIBuffer buf = queryTraCI(CMD_SET_VEHICLE_VARIABLE, TraCIBuffer() << variableId << nodeId << variableType << value);
    ASSERT(buf.eof());
}


// this changes vehicle type!
void TraCI_Extend::commandSetTg(std::string nodeId, double value)
{
    uint8_t variableId = VAR_TAU;
    uint8_t variableType = TYPE_DOUBLE;

    TraCIBuffer buf = queryTraCI(CMD_SET_VEHICLE_VARIABLE, TraCIBuffer() << variableId << nodeId << variableType << value);
    ASSERT(buf.eof());
}


void TraCI_Extend::commandAddVehicleN(std::string vehicleId, std::string vehicleTypeId, std::string routeId, int32_t depart, double pos, double speed, uint8_t lane)
{
    uint8_t variableId = ADD;
    uint8_t variableType = TYPE_COMPOUND;
    uint8_t variableTypeS = TYPE_STRING;
    uint8_t variableTypeI = TYPE_INTEGER;
    uint8_t variableTypeD = TYPE_DOUBLE;
    uint8_t variableTypeB = TYPE_BYTE;

    TraCIBuffer buf = queryTraCI(CMD_SET_VEHICLE_VARIABLE, TraCIBuffer() << variableId << vehicleId
                                                                         << variableType << (int32_t) 6
                                                                         << variableTypeS
                                                                         << vehicleTypeId
                                                                         << variableTypeS
                                                                         << routeId
                                                                         << variableTypeI
                                                                         << depart        // departure time
                                                                         << variableTypeD
                                                                         << pos           // departure position
                                                                         << variableTypeD
                                                                         << speed         // departure speed
                                                                         << variableTypeB
                                                                         << lane          // departure lane
                                                                         );
    ASSERT(buf.eof());
}


void TraCI_Extend::commandSetCFParameters(std::string nodeId, std::string value)
{
    uint8_t variableId = 0x15;
    uint8_t variableType = TYPE_STRING;

    TraCIBuffer buf = queryTraCI(CMD_SET_VEHICLE_VARIABLE, TraCIBuffer() << variableId << nodeId << variableType << value);
    ASSERT(buf.eof());
}


void TraCI_Extend::commandSetDebug(std::string nodeId, bool value)
{
    uint8_t variableId = 0x16;
    uint8_t variableType = TYPE_INTEGER;

    TraCIBuffer buf = queryTraCI(CMD_SET_VEHICLE_VARIABLE, TraCIBuffer() << variableId << nodeId << variableType << (int)value);
    ASSERT(buf.eof());
}


void TraCI_Extend::commandSetModeSwitch(std::string nodeId, bool value)
{
    uint8_t variableId = 0x17;
    uint8_t variableType = TYPE_INTEGER;

    TraCIBuffer buf = queryTraCI(CMD_SET_VEHICLE_VARIABLE, TraCIBuffer() << variableId << nodeId << variableType << (int)value);
    ASSERT(buf.eof());
}


void TraCI_Extend::commandSetVehicleColor(std::string nodeId, TraCIColor& color)
{
   TraCIBuffer p;
   p << static_cast<uint8_t>(VAR_COLOR);
   p << nodeId;
   p << static_cast<uint8_t>(TYPE_COLOR) << color.red << color.green << color.blue << color.alpha;
   TraCIBuffer buf = queryTraCI(CMD_SET_VEHICLE_VARIABLE, p);

   ASSERT(buf.eof());
 }


void TraCI_Extend::commandRemoveVehicle(std::string nodeId, uint8_t reason)
{
    uint8_t variableId = REMOVE;
    uint8_t variableType = TYPE_BYTE;

    TraCIBuffer buf = queryTraCI(CMD_SET_VEHICLE_VARIABLE, TraCIBuffer() << variableId << nodeId << variableType << reason);
    ASSERT(buf.eof());

    activeVehicleCount--;
    TraCIScenarioManager::deleteModule(nodeId);
}


void TraCI_Extend::commandStopNodeExtended(std::string nodeId, std::string edgeId, double stopPos, uint8_t laneId, double waitT, uint8_t flag)
{
    uint8_t variableId = CMD_STOP;
    uint8_t variableType = TYPE_COMPOUND;
    int32_t count = 5;
    uint8_t edgeIdT = TYPE_STRING;
    uint8_t stopPosT = TYPE_DOUBLE;
    uint8_t stopLaneT = TYPE_BYTE;
    uint8_t durationT = TYPE_INTEGER;
    uint32_t duration = waitT * 1000;
    uint8_t flagT = TYPE_BYTE;

    TraCIBuffer buf = queryTraCI(CMD_SET_VEHICLE_VARIABLE, TraCIBuffer() << variableId << nodeId
                                                                                       << variableType << count
                                                                                       << edgeIdT << edgeId
                                                                                       << stopPosT << stopPos
                                                                                       << stopLaneT << laneId
                                                                                       << durationT << duration
                                                                                       << flagT << flag
                                                                                       );
    ASSERT(buf.eof());
}


void TraCI_Extend::commandSetvClass(std::string nodeId, std::string vClass)
{
    uint8_t variableId = VAR_VEHICLECLASS;
    uint8_t variableType = TYPE_STRING;

    TraCIBuffer buf = queryTraCI(CMD_SET_VEHICLE_VARIABLE, TraCIBuffer() << variableId << nodeId << variableType << vClass);
    ASSERT(buf.eof());
}


void TraCI_Extend::commandChangeLane(std::string nodeId, uint8_t laneId, double duration)
{
    uint8_t variableId = CMD_CHANGELANE;
    uint8_t variableType = TYPE_COMPOUND;
    int32_t count = 2;
    uint8_t laneIdT = TYPE_BYTE;
    uint8_t durationT = TYPE_INTEGER;
    uint32_t durationMS = duration * 1000;

    TraCIBuffer buf = queryTraCI(CMD_SET_VEHICLE_VARIABLE, TraCIBuffer() << variableId << nodeId
                                                                                       << variableType << count
                                                                                       << laneIdT << laneId
                                                                                       << durationT << durationMS
                                                                                       );
    ASSERT(buf.eof());
}


void TraCI_Extend::commandSetErrorGap(std::string nodeId, double value)
{
    uint8_t variableId = 0x20;
    uint8_t variableType = TYPE_DOUBLE;

    TraCIBuffer buf = queryTraCI(CMD_SET_VEHICLE_VARIABLE, TraCIBuffer() << variableId << nodeId << variableType << value);
    ASSERT(buf.eof());
}


void TraCI_Extend::commandSetErrorRelSpeed(std::string nodeId, double value)
{
    uint8_t variableId = 0x21;
    uint8_t variableType = TYPE_DOUBLE;

    TraCIBuffer buf = queryTraCI(CMD_SET_VEHICLE_VARIABLE, TraCIBuffer() << variableId << nodeId << variableType << value);
    ASSERT(buf.eof());
}


// #####################
// CMD_SET_GUI_VARIABLE
// #####################

void TraCI_Extend::commandSetGUIZoom(double value)
{
    uint8_t variableId = VAR_VIEW_ZOOM;
    std::string viewID = "View #0";
    uint8_t variableType = TYPE_DOUBLE;

    TraCIBuffer buf = queryTraCI(CMD_SET_GUI_VARIABLE, TraCIBuffer() << variableId << viewID << variableType << value);
    ASSERT(buf.eof());
}


// very slow!
void TraCI_Extend::commandSetGUITrack(std::string nodeId)
{
    uint8_t variableId = VAR_TRACK_VEHICLE;
    std::string viewID = "View #0";
    uint8_t variableType = TYPE_STRING;

    TraCIBuffer buf = queryTraCI(CMD_SET_GUI_VARIABLE, TraCIBuffer() << variableId << viewID << variableType << nodeId);
    ASSERT(buf.eof());
}


void TraCI_Extend::commandSetGUIOffset(double x, double y)
{
    uint8_t variableId = VAR_VIEW_OFFSET;
    std::string viewID = "View #0";
    uint8_t variableType = POSITION_2D;

    TraCIBuffer buf = queryTraCI(CMD_SET_GUI_VARIABLE, TraCIBuffer() << variableId << viewID << variableType << x << y);
    ASSERT(buf.eof());
}


// #####################
// Polygon
// #####################

void TraCI_Extend::commandAddCirclePoly(std::string name, std::string type, TraCIColor color, Coord center, double radius)
{
    std::list<Coord> circlePoints;

    // Convert from degrees to radians via multiplication by PI/180
    for(int angleInDegrees = 0; angleInDegrees <= 360; angleInDegrees = angleInDegrees + 10)
    {
        double x = (double)( radius * std::cos(angleInDegrees * 3.14 / 180) ) + center.x;
        double y = (double)( radius * std::sin(angleInDegrees * 3.14 / 180) ) + center.y;

        Coord *p = new Coord(x, y);
        circlePoints.push_back(p);
    }

    // create polygon in SUMO
    commandAddPolygon(name, type, color, 0, 1, circlePoints);
}


void TraCI_Extend::finish()
{

}

