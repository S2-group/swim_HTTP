/*******************************************************************************
 * Simulator of Web Infrastructure and Management
 * Copyright (c) 2016 Carnegie Mellon University.
 * All Rights Reserved.
 *  
 * THIS SOFTWARE IS PROVIDED "AS IS," WITH NO WARRANTIES WHATSOEVER. CARNEGIE
 * MELLON UNIVERSITY EXPRESSLY DISCLAIMS TO THE FULLEST EXTENT PERMITTED BY LAW
 * ALL EXPRESS, IMPLIED, AND STATUTORY WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, AND NON-INFRINGEMENT OF PROPRIETARY RIGHTS.
 *  
 * Released under a BSD license, please see license.txt for full terms.
 * DM-0003883
 *******************************************************************************/
#ifndef __SWIM_HTTPINTERFACE_H_
#define __SWIM_HTTPINTERFACE_H_

#include "SocketRTScheduler.h"
#include <omnetpp.h>
#include <string>
#include <vector>
#include <functional>
#include <map>
#include "model/Model.h"
#include "managers/monitor/IProbe.h"
#include "QueueBase.h"

/**
 * Adaptation interface (probes and effectors)
 */
class HTTPInterface : public omnetpp::cSimpleModule
{
public:
    HTTPInterface();
    virtual ~HTTPInterface();

protected:
    std::map<std::string, std::function<std::string(const std::vector<std::string>&)>> commandHandlers;
    Model* pModel;
    IProbe* pProbe;

    virtual void initialize();
    virtual void handleMessage(cMessage *msg);

    virtual std::string cmdAddServer(const std::vector<std::string>& args);
    virtual std::string cmdRemoveServer(const std::vector<std::string>& args);
    virtual std::string cmdSetDimmer(const std::vector<std::string>& args);

    virtual std::string cmdGetDimmer(const std::vector<std::string>& args);
    virtual std::string cmdGetServers(const std::vector<std::string>& args);
    virtual std::string cmdGetActiveServers(const std::vector<std::string>& args);
    virtual std::string cmdGetMaxServers(const std::vector<std::string>& args);
    virtual std::string cmdGetUtilization(const std::vector<std::string>& args);
    virtual std::string cmdGetBasicResponseTime(const std::vector<std::string>& args);
    virtual std::string cmdGetBasicThroughput(const std::vector<std::string>& args);
    virtual std::string cmdGetOptResponseTime(const std::vector<std::string>& args);
    virtual std::string cmdGetOptThroughput(const std::vector<std::string>& args);
    virtual std::string cmdGetArrivalRate(const std::vector<std::string>& args);

private:
    static const unsigned BUFFER_SIZE = 4000;
    cMessage *rtEvent;
    cSocketRTScheduler *rtScheduler;

    char recvBuffer[BUFFER_SIZE];
    int numRecvBytes;

    std::vector<std::string> monitor = {
      "dimmer",
      "servers",
      "active_servers",
      "max_servers",
      "utilization", 
      "basic_rt",
      "basic_throughput",
      "opt_rt",
      "opt_throughput",
      "arrival_rate"
    };

    std::vector<std::string> adaptations = {
      "/add_server",
      "/remove_server",
      "/set_dimmer"
    };

};

#endif
