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
#include <boost/tokenizer.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
/**
 * Adaptation interface (probes and effectors)
 */
class HTTPInterface : public omnetpp::cSimpleModule
{
public:
    HTTPInterface();
    virtual ~HTTPInterface();

protected:
    std::map<std::string, std::function<std::string(const std::string&)>> commandHandlers;
    std::map<std::string, std::function<std::string(const std::string&)>> endpointGETHandlers;
    std::map<std::string, std::function<std::string(const std::string&)>> endpointPUTHandlers;
    std::map< std::string, std::map<std::string, std::function<std::string(const std::string&)>> > HTTPAPI;
    Model* pModel;
    IProbe* pProbe;

    virtual void initialize();
    virtual void handleMessage(cMessage *msg);

    virtual std::string epMonitor(const std::string& arg);
    virtual std::string epMonitorSchema(const std::string& arg);
    virtual std::string epExecuteSchema(const std::string& arg);
    virtual std::string epAdapOptions(const std::string& arg);
    virtual std::string epAdapOptSchema(const std::string& arg);
    virtual std::string epAdaptationOps(const std::string& arg);
    virtual std::string epExecute(const std::string& arg);


//    virtual std::string cmdSetServers(const std::string& arg);
//    virtual std::string cmdAddServer(const std::string& arg);
//    virtual std::string cmdRemoveServer(const std::string& arg);
//    virtual std::string cmdSetDimmer(const std::string& arg);
//
//    virtual std::string cmdGetDimmer(const std::string& arg);
//    virtual std::string cmdGetServers(const std::string& arg);
//    virtual std::string cmdGetActiveServers(const std::string& arg);
//    virtual std::string cmdGetMaxServers(const std::string& arg);
//    virtual std::string cmdGetUtilization(const std::string& arg);
//    virtual std::string cmdGetBasicResponseTime(const std::string& arg);
//    virtual std::string cmdGetBasicThroughput(const std::string& arg);
//    virtual std::string cmdGetOptResponseTime(const std::string& arg);
//    virtual std::string cmdGetOptThroughput(const std::string& arg);
//    virtual std::string cmdGetArrivalRate(const std::string& arg);

private:
    static const unsigned BUFFER_SIZE = 4000;
    cMessage *rtEvent;
    cSocketRTScheduler *rtScheduler;

    std::string http_rq_type;
    std::string http_rq_body;
    std::string http_rq_endpoint;

    std::string response_body;
    std::string status_code;
    boost::property_tree::ptree temp_json;

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
      "dimmer_number"
    };
};

#endif
