
#define WANT_WINSOCK2
#include <platdep/sockets.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32) || defined(__CYGWIN__) || defined(_WIN64)
#include <ws2tcpip.h>
#else
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif

#include <algorithm>
#include <functional>
#include <thread>

#include "TraCIConnection.h"
#include "TraCIConstants.h"
#include "TraCICommands.h"
#include "vlog.h"


namespace VENTOS {

pid_t TraCIConnection::child_pid = -1;

SOCKET socket(void* ptr)
{
    ASSERT(ptr);
    return *static_cast<SOCKET*>(ptr);
}


TraCIConnection::TraCIConnection(void* ptr) : socketPtr(ptr)
{
    ASSERT(socketPtr);
}


TraCIConnection::~TraCIConnection()
{
    if (socketPtr)
    {
        closesocket(socket(socketPtr));
        delete static_cast<SOCKET*>(socketPtr);
    }

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32) || defined(__CYGWIN__) || defined(_WIN64)
#else
    // send SIGINT
    if (child_pid > 0)
        kill(child_pid, 15);
#endif
}


int TraCIConnection::startSUMO(std::string SUMOexe, std::string SUMOconfig, std::string switches, int seed)
{
    int port = TraCIConnection::getFreeEphemeralPort();

    // auto-set seed, if requested
    if (seed == -1)
    {
        const char* seed_s = omnetpp::getEnvir()->getConfigEx()->getVariable(CFGVAR_RUNNUMBER);
        seed = atoi(seed_s);
    }

    LOG_INFO << "\n>>> Starting SUMO process ... \n";
    LOG_INFO << boost::format("    Executable file: %1% \n") % SUMOexe;
    LOG_INFO << boost::format("    Config file: %1% \n") % SUMOconfig;
    LOG_INFO << boost::format("    Switches: %1% \n") % switches;
    LOG_INFO << boost::format("    TraCI server: port= %1%, seed= %2% \n") % port % seed;
    LOG_FLUSH;

    // assemble commandLine
    std::ostringstream commandLine;
    commandLine << SUMOexe
            << " --remote-port " << port
            << " --seed " << seed
            << " --configuration-file " << SUMOconfig
            << switches;

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32) || defined(__CYGWIN__) || defined(_WIN64)
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFOA);

    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

    char cmdline[32768];
    strncpy(cmdline, commandLine.c_str(), sizeof(cmdline));
    bool bSuccess = CreateProcess(0, cmdline, 0, 0, 1, NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE, 0, 0, &si, &pi);
    if (!bSuccess)
    {
        std::string msg = "undefined error";

        DWORD errorMessageID = ::GetLastError();
        if(errorMessageID != 0)
        {

            LPSTR messageBuffer = nullptr;
            size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

            std::string message(messageBuffer, size);
            LocalFree(messageBuffer);
            msg = message;
        }

        msg = std::string() + "Error launching TraCI server (\"" + commandLine + "\"): " + msg + ". Make sure you have set $PATH correctly.";

        throw cRuntimeError(msg.c_str());
    }
    else
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

#else

    // create a child process
    // forking creates an exact copy of the parent process at the time of forking.
    child_pid = fork();

    // fork failed
    if(child_pid < 0)
        throw omnetpp::cRuntimeError("fork failed!");

    // in child process
    if(child_pid == 0)
    {
        // make the child process ignore the SIGINT signal
        signal(SIGINT, SIG_IGN);

        // run SUMO server inside this child process
        // if execution is successful then child will be blocked at this line
        int r = system(commandLine.str().c_str());

        if (r == -1)
            throw omnetpp::cRuntimeError("Running \"%s\" failed during system()", commandLine.str().c_str());

        if (WEXITSTATUS(r) != 0)
            throw omnetpp::cRuntimeError("Error launching TraCI server (\"%s\"): exited with code %d.", commandLine.str().c_str(), WEXITSTATUS(r));

        exit(1);
    }
    else
    {
        LOG_INFO << boost::format("    SUMO has started successfully in process %1%  \n") % child_pid;
        LOG_FLUSH;
    }

#endif

    return port;
}


// get a random Ephemeral port from the system
int TraCIConnection::getFreeEphemeralPort()
{
    if (initsocketlibonce() != 0)
        throw omnetpp::cRuntimeError("Could not init socketlib");

    SOCKET sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        throw omnetpp::cRuntimeError("Failed to create socket: %s", strerror(errno));

    struct sockaddr_in serv_addr;
    struct sockaddr* serv_addr_p = (struct sockaddr*)&serv_addr;
    memset(serv_addr_p, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = 0;   // get a random Ephemeral port

    if (::bind(sock, serv_addr_p, sizeof(serv_addr)) < 0)
        throw omnetpp::cRuntimeError("Failed to bind socket: %s", strerror(errno));

    socklen_t len = sizeof(serv_addr);
    if (getsockname(sock, serv_addr_p, &len) < 0)
        throw omnetpp::cRuntimeError("Failed to get hostname: %s", strerror(errno));

    int port = ntohs(serv_addr.sin_port);

    closesocket(sock);

    return port;
}


TraCIConnection* TraCIConnection::connect(const char* host, int port)
{
    LOG_INFO << boost::format("\n>>> Connecting to TraCI server on port %1% ... \n") % port << std::flush;

    if (initsocketlibonce() != 0)
        throw omnetpp::cRuntimeError("Could not init socketlib");

    in_addr addr;
    struct hostent* host_ent;
    struct in_addr saddr;

    saddr.s_addr = inet_addr(host);
    if (saddr.s_addr != static_cast<unsigned int>(-1))
        addr = saddr;
    else if ((host_ent = gethostbyname(host)))
        addr = *((struct in_addr*) host_ent->h_addr_list[0]);
    else
        throw omnetpp::cRuntimeError("Invalid TraCI server address: %s", host);

    sockaddr_in address;
    sockaddr* address_p = (sockaddr*)&address;
    memset(address_p, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = addr.s_addr;

    SOCKET* socketPtr = new SOCKET();
    if (*socketPtr < 0)
        throw omnetpp::cRuntimeError("Could not create socket to connect to TraCI server");

    // wait for 1 second and then try connecting to TraCI server
    std::this_thread::sleep_for(std::chrono::seconds(1));

    int tries = 1;
    for (; tries <= 10; ++tries)
    {
        *socketPtr = ::socket(AF_INET, SOCK_STREAM, 0);
        if (::connect(*socketPtr, address_p, sizeof(address)) >= 0)
            break;

        closesocket(socket(socketPtr));

        int sleepDuration = tries * .25 + 1;

        LOG_INFO << boost::format("    Could not connect to the TraCI server: %1% -- retry in %2% seconds. \n") % strerror(sock_errno()) % sleepDuration << std::flush;

        std::this_thread::sleep_for(std::chrono::seconds(sleepDuration));
    }

    if(tries == 11)
        throw omnetpp::cRuntimeError("Could not connect to TraCI server after 10 retries!");

    // TCP_NODELAY: disable the Nagle algorithm. This means that segments are always
    // sent as soon as possible, even if there is only a small amount of data.
    // When not set, data is buffered until there is a sufficient amount to send out,
    // thereby avoiding the frequent sending of small packets, which results
    // in poor utilization of the network.
    int x = 1;
    ::setsockopt(*socketPtr, IPPROTO_TCP, TCP_NODELAY, (const char*) &x, sizeof(x));

    return new TraCIConnection(socketPtr);
}


TraCIBuffer TraCIConnection::query(uint8_t commandGroupId, const TraCIBuffer& buf)
{
    sendMessage(makeTraCICommand(commandGroupId, buf));

    TraCIBuffer obuf(receiveMessage());
    uint8_t cmdLength; obuf >> cmdLength;
    uint8_t commandResp; obuf >> commandResp;
    ASSERT(commandResp == commandGroupId);
    uint8_t result; obuf >> result;
    std::string description; obuf >> description;

    if (result == RTYPE_NOTIMPLEMENTED)
        throw omnetpp::cRuntimeError("TraCI server reported command 0x%2x not implemented (\"%s\"). Might need newer version.", commandGroupId, description.c_str());

    if (result == RTYPE_ERR)
        throw omnetpp::cRuntimeError("TraCI server reported throw cRuntimeError executing command 0x%2x (\"%s\").", commandGroupId, description.c_str());

    ASSERT(result == RTYPE_OK);

    return obuf;
}


TraCIBuffer TraCIConnection::queryOptional(uint8_t commandGroupId, const TraCIBuffer& buf, bool& success, std::string* errorMsg)
{
    sendMessage(makeTraCICommand(commandGroupId, buf));

    TraCIBuffer obuf(receiveMessage());
    uint8_t cmdLength; obuf >> cmdLength;
    uint8_t commandResp; obuf >> commandResp;
    ASSERT(commandResp == commandGroupId);
    uint8_t result; obuf >> result;
    std::string description; obuf >> description;
    success = (result == RTYPE_OK);
    if (errorMsg)
        *errorMsg = description;

    return obuf;
}


std::string TraCIConnection::receiveMessage()
{
    if (!socketPtr)
        throw omnetpp::cRuntimeError("Not connected to TraCI server");

    uint32_t msgLength;

    {
        char buf2[sizeof(uint32_t)];
        uint32_t bytesRead = 0;
        while (bytesRead < sizeof(uint32_t))
        {
            int receivedBytes = ::recv(socket(socketPtr), reinterpret_cast<char*>(&buf2) + bytesRead, sizeof(uint32_t) - bytesRead, 0);
            if (receivedBytes > 0)
                bytesRead += receivedBytes;
            else if (receivedBytes == 0)
                terminateSimulation("ERROR in receiveMessage: Connection to TraCI server closed unexpectedly. \n\n");
            else
            {
                if (sock_errno() == EINTR) continue;
                if (sock_errno() == EAGAIN) continue;

                terminateSimulation("ERROR in receiveMessage: Connection to TraCI server closed unexpectedly. \n\n");
            }
        }

        TraCIBuffer(std::string(buf2, sizeof(uint32_t))) >> msgLength;
    }

    uint32_t bufLength = msgLength - sizeof(msgLength);
    char buf[bufLength];

    {
        uint32_t bytesRead = 0;
        while (bytesRead < bufLength)
        {
            int receivedBytes = ::recv(socket(socketPtr), reinterpret_cast<char*>(&buf) + bytesRead, bufLength - bytesRead, 0);
            if (receivedBytes > 0)
                bytesRead += receivedBytes;
            else if (receivedBytes == 0)
                terminateSimulation("ERROR in receiveMessage: Connection to TraCI server closed unexpectedly. \n\n");
            else
            {
                if (sock_errno() == EINTR) continue;
                if (sock_errno() == EAGAIN) continue;

                terminateSimulation("ERROR in receiveMessage: Connection to TraCI server closed unexpectedly. \n\n");
            }
        }
    }

    return std::string(buf, bufLength);
}


void TraCIConnection::sendMessage(std::string buf)
{
    if (!socketPtr)
        throw omnetpp::cRuntimeError("Not connected to TraCI server");

    {
        uint32_t msgLength = sizeof(uint32_t) + buf.length();
        TraCIBuffer buf2 = TraCIBuffer();
        buf2 << msgLength;
        uint32_t bytesWritten = 0;
        while (bytesWritten < sizeof(uint32_t))
        {
            size_t sentBytes = ::send(socket(socketPtr), buf2.str().c_str() + bytesWritten, sizeof(uint32_t) - bytesWritten, 0);
            if (sentBytes > 0)
                bytesWritten += sentBytes;
            else
            {
                if (sock_errno() == EINTR) continue;
                if (sock_errno() == EAGAIN) continue;

                terminateSimulation("ERROR in receiveMessage: Connection to TraCI server closed unexpectedly. \n\n");
            }
        }
    }

    {
        uint32_t bytesWritten = 0;
        while (bytesWritten < buf.length())
        {
            size_t sentBytes = ::send(socket(socketPtr), buf.c_str() + bytesWritten, buf.length() - bytesWritten, 0);
            if (sentBytes > 0)
                bytesWritten += sentBytes;
            else
            {
                if (sock_errno() == EINTR) continue;
                if (sock_errno() == EAGAIN) continue;

                terminateSimulation("ERROR in receiveMessage: Connection to TraCI server closed unexpectedly. \n\n");
            }
        }
    }
}


std::string makeTraCICommand(uint8_t commandId, const TraCIBuffer& buf)
{
    if (sizeof(uint8_t) + sizeof(uint8_t) + buf.str().length() > 0xFF)
    {
        uint32_t len = sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint8_t) + buf.str().length();
        return (TraCIBuffer() << static_cast<uint8_t>(0) << len << commandId).str() + buf.str();
    }

    uint8_t len = sizeof(uint8_t) + sizeof(uint8_t) + buf.str().length();
    return (TraCIBuffer() << len << commandId).str() + buf.str();
}


void TraCIConnection::terminateSimulation(std::string err)
{
    LOG_ERROR << "\n" << err << std::flush;

    // get a pointer to TraCI module
    omnetpp::cModule *module = omnetpp::getSimulation()->getSystemModule()->getSubmodule("TraCI");
    ASSERT(module);
    TraCI_Commands *TraCI = static_cast<TraCI_Commands *>(module);
    ASSERT(TraCI);

    // set TraCIclosed flag that is used in the finish()
    TraCI->par("TraCIclosed") = true;

    // end the simulation
    TraCI->endSimulation();
}

}
