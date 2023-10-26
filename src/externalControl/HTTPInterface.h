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
#include <boost/property_tree/ptree.hpp>
#include <functional>
#include <map>
#include "model/Model.h"
#include "managers/monitor/IProbe.h"

/**
 * Adaptation interface (probes and effectors)
 */
class HTTPInterface : public omnetpp::cSimpleModule
{
public:
    HTTPInterface();
    virtual ~HTTPInterface();

protected:
    std::map<std::string, std::function<void(boost::property_tree::ptree&)>> monitor_mapping;

    Model* pModel;
    IProbe* pProbe;

    virtual void initialize();
    virtual void handleMessage(cMessage *msg);

    virtual std::string cmdSetServers(const std::string& arg);
    virtual std::string cmdSetDimmer(const std::string& arg);

    virtual std::string cmdGetDimmer(const std::string& arg);
    virtual std::string cmdGetServers(const std::string& arg);
    virtual std::string cmdGetMaxServers(const std::string& arg);

    virtual void addDimmer(boost::property_tree::ptree& json);
    virtual void addServers(boost::property_tree::ptree& json);
    virtual void addActiveServers(boost::property_tree::ptree& json);
    virtual void addMaxServers(boost::property_tree::ptree& json);
    virtual void addUtilization(boost::property_tree::ptree& json);
    virtual void addBasicResponseTime(boost::property_tree::ptree& json);
    virtual void addBasicThroughput(boost::property_tree::ptree& json);
    virtual void addOptResponseTime(boost::property_tree::ptree& json);
    virtual void addOptThroughput(boost::property_tree::ptree& json);
    virtual void addArrivalRate(boost::property_tree::ptree& json);

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
      "server_number",
      "dimmer_factor"
    };
};

#endif
