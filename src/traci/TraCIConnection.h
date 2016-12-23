#ifndef TRACICONNECTION_H_
#define TRACICONNECTION_H_

#include <stdint.h>
#include "Coord.h"

#include "TraCIBuffer.h"
#include "TraCICoord.h"

namespace VENTOS {

class TraCIConnection
{
public:
    static int startSUMO(std::string SUMOexe, std::string SUMOconfig, std::string switches, int seed);
    static int getFreeEphemeralPort();
    static TraCIConnection* connect(const char* host, int port);
    ~TraCIConnection();

    /**
     * sends a single command via TraCI, checks status response, returns additional responses
     */
    TraCIBuffer query(uint8_t commandId, const TraCIBuffer& buf = TraCIBuffer());

    /**
     * sends a single command via TraCI, expects no reply, returns true if successful
     */
    TraCIBuffer queryOptional(uint8_t commandId, const TraCIBuffer& buf, bool& success, std::string* errorMsg = 0);

    /**
     * sends a message via TraCI (after adding the header)
     */
    void sendMessage(std::string buf);

    /**
     * receives a message via TraCI (and strips the header)
     */
    std::string receiveMessage();

private:
    TraCIConnection(void*);
    void terminateSimulation(std::string);
    static std::string getSUMOversion(std::string path);

private:
    static void* socketPtr;
    static pid_t child_pid;
};

/**
 * returns byte-buffer containing a TraCI command with optional parameters
 */
std::string makeTraCICommand(uint8_t commandId, const TraCIBuffer& buf = TraCIBuffer());

}

#endif /* VEINS_MOBILITY_TRACI_TRACICONNECTION_H_ */
