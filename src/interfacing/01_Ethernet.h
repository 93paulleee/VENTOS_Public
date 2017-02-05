/****************************************************************************/
/// @file    Ethernet.h
/// @author  Mani Amoozadeh <maniam@ucdavis.edu>
/// @author  second author name
/// @date    Dec 2015
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

#ifndef SNIFFETHERNET_H
#define SNIFFETHERNET_H

#include <pcap.h>
#include <mutex>

#include <arpa/inet.h>     // inet_ntoa
#include <ifaddrs.h>       // ifaddrs
#include <net/ethernet.h>  // ether_header, ETHER_ADDR_LEN, ETHER_HDR_LEN

# define __FAVOR_BSD
#include <netinet/if_ether.h>    // ether_arp
#include <netinet/ip.h>          // iphdr
#include <netinet/ip6.h>         // ipv6hdr
#include <netinet/tcp.h>         // tcphdr
#include <netinet/udp.h>         // udphdr
#include <netinet/ip_icmp.h>     // icmphdr

#include "veins/modules/BaseApplLayer.h"
#include "traci/TraCICommands.h"

namespace VENTOS {

/* default snap length (maximum bytes per packet to capture) */
#define SNAP_LEN 1518

class devDesc
{
public:
    uint32_t address;
    uint32_t netMask;

    devDesc(uint32_t x, uint32_t y)
    {
        this->address = x;
        this->netMask = y;
    }
};


class Ethernet : public BaseApplLayer
{
private:
    typedef BaseApplLayer super;

    // NED variables
    bool active;
    std::string interface;
    std::string filter_exp;
    bool printCaptured;
    bool printDataPayload;
    bool printStat;

    // variables
    TraCI_Commands *TraCI = NULL;
    omnetpp::simsignal_t Signal_initialize_withTraCI;
    omnetpp::simsignal_t Signal_executeEachTS;

    pcap_t *pcap_handle = NULL;
    std::mutex vectorLock;
    std::map<std::string, devDesc> allDev;
    std::vector< std::pair<struct pcap_pkthdr *, const u_char *> > framesQueue;

public:
    virtual ~Ethernet();
    virtual void finish();
    virtual void initialize(int);
    virtual void handleMessage(omnetpp::cMessage *);
    virtual void receiveSignal(omnetpp::cComponent *, omnetpp::simsignal_t, long, cObject* details);

private:
    void initialize_withTraCI();
    void executeEachTimestep();

    void listInterfaces();

    void initSniffing();
    void startSniffing();
    void process_packet(const struct pcap_pkthdr *header, const u_char *packet);

    void processARP(const u_char *packet);
    void processIPv4(const u_char *packet);
    void processIPv6(const u_char *packet);
    void processTCP(const u_char *packet, const struct ip *);
    void processUDP(const u_char *packet, const struct ip *);
    void processICMP(const u_char *packet, const struct ip *);

    void print_dataPayload(const u_char *payload, int len);
    void print_hex_ascii_line(const u_char *payload, int len, int offset);
};

}

#endif
