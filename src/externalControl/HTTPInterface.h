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
    std::map<std::string, int*> integerMonitorable;
    std::map<std::string, double*> doubleMonitorable;

    std::map<std::string, std::function<bool(const std::string&)>> endpointGETHandlers;
    std::map<std::string, std::function<bool(const std::string&)>> endpointPUTHandlers;
    std::map<std::string, std::map<std::string, std::function<bool(const std::string&)>> > HTTPAPI;
    Model* pModel;
    IProbe* pProbe;

    virtual void initialize();
    virtual void handleMessage(cMessage *msg);
    virtual bool parseMessage();
    virtual void sendResponse(const std::string& status_code, const std::string& response_body);
    virtual void updateMonitoring();
    virtual std::string cmdSetServers(const std::string& arg);
    virtual std::string cmdSetDimmer(const std::string& arg);

    boost::property_tree::ptree allUtilization();

    template <class T>
    void putInJSON(boost::property_tree::ptree& json_file, std::map<std::string, T*>& some_map);


    virtual bool epMonitor(const std::string& arg);
    virtual bool epMonitorSchema(const std::string& arg);
    virtual bool epExecuteSchema(const std::string& arg);
    virtual bool epAdapOptions(const std::string& arg);
    virtual bool epAdapOptSchema(const std::string& arg);
    virtual bool epExecute(const std::string& arg);


private:
    static const unsigned BUFFER_SIZE = 4000;
    cMessage *rtEvent;
    cSocketRTScheduler *rtScheduler;

    std::string http_rq_type;
    std::string http_rq_body;
    std::string http_rq_endpoint;
    std::vector<std::string> lines;
    std::string response_body;
    std::string status_code;
    boost::property_tree::ptree response_json;

    double dimmer_factor;
    int servers;
    int active_servers;
    double basic_rt;
    int max_servers;
    double basic_throughput;
    double opt_throughput;
    double opt_rt;
    double arrival_rate;
    boost::property_tree::ptree utilization;

    char recvBuffer[BUFFER_SIZE];
    int numRecvBytes;


    std::vector<int*> variableVector = {&servers, &max_servers, &active_servers};


    std::vector<std::string> monitorable_list = {
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
