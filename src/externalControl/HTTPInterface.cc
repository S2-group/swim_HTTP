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
#include "HTTPInterface.h"
#include <string>
#include <map>
#include <sstream>
#include <boost/tokenizer.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <managers/execution/ExecutionManagerMod.h>

Define_Module(HTTPInterface);

#define DEBUG_HTTP_INTERFACE 0

using namespace std;

namespace {
    const string UNKNOWN_COMMAND = "error: unknown command\n";
    const string COMMAND_SUCCESS = "OK\n";
}

HTTPInterface::HTTPInterface() {
    // put commands
    commandHandlers["set_server"] = std::bind(&HTTPInterface::cmdSetServer, this, std::placeholders::_1);
    commandHandlers["set_dimmer"] = std::bind(&HTTPInterface::cmdSetDimmer, this, std::placeholders::_1);

    // get commands
    commandHandlers["dimmer"] = std::bind(&HTTPInterface::cmdGetDimmer, this, std::placeholders::_1);
    commandHandlers["servers"] = std::bind(&HTTPInterface::cmdGetServers, this, std::placeholders::_1);
    commandHandlers["active_servers"] = std::bind(&HTTPInterface::cmdGetActiveServers, this, std::placeholders::_1);
    commandHandlers["max_servers"] = std::bind(&HTTPInterface::cmdGetMaxServers, this, std::placeholders::_1);
    commandHandlers["utilization"] = std::bind(&HTTPInterface::cmdGetUtilization, this, std::placeholders::_1);
    commandHandlers["basic_rt"] = std::bind(&HTTPInterface::cmdGetBasicResponseTime, this, std::placeholders::_1);
    commandHandlers["basic_throughput"] = std::bind(&HTTPInterface::cmdGetBasicThroughput, this, std::placeholders::_1);
    commandHandlers["opt_rt"] = std::bind(&HTTPInterface::cmdGetOptResponseTime, this, std::placeholders::_1);
    commandHandlers["opt_throughput"] = std::bind(&HTTPInterface::cmdGetOptThroughput, this, std::placeholders::_1);
    commandHandlers["arrival_rate"] = std::bind(&HTTPInterface::cmdGetArrivalRate, this, std::placeholders::_1);

    // dimmer, numServers, numActiveServers, utilization(total or indiv), response time and throughput for mandatory and optional, avg arrival rate
}

HTTPInterface::~HTTPInterface() {
    cancelAndDelete(rtEvent);
}

void HTTPInterface::initialize()
{
    rtEvent = new cMessage("rtEvent");
    rtScheduler = check_and_cast<cSocketRTScheduler *>(getSimulation()->getScheduler());
    rtScheduler->setInterfaceModule(this, rtEvent, recvBuffer, BUFFER_SIZE, &numRecvBytes);
    pModel = check_and_cast<Model*> (getParentModule()->getSubmodule("model"));
    pProbe = check_and_cast<IProbe*> (gate("probe")->getPreviousGate()->getOwnerModule());
}

void HTTPInterface::handleMessage(cMessage *msg) {
    if (msg != rtEvent) {
        // Handle the message only if it's the expected event
        return;
    }

    // Get data from the buffer
    std::string input(recvBuffer, numRecvBytes);
    numRecvBytes = 0;
    char recvBuffer[BUFFER_SIZE];

    std::vector<std::string> words;
    std::istringstream iss(input);
    for (std::string word; iss >> word;) {
        words.push_back(word);
    }

    std::stringstream ss(input);
    std::vector<std::string> lines;
    std::string line;

    while (std::getline(ss, line, '\n')) {
        lines.push_back(line);
    }

    std::string response_body = "";
    std::string status_code = "200 OK";
    std::string response_type;

    if (words.empty()) {
        // Handle the case where there are no words
        status_code = "400 Bad Request";
    } else if (words[0] == "GET") {
        boost::property_tree::ptree temp_json;

        response_type = "application/json";

        if (words[1] == "/monitor") {
            for (const std::string& monitorable : monitor) {
                // Add key-value pairs to the JSON object
                temp_json.put(monitorable, commandHandlers[monitorable](std::string()));
            }
        } else if (words[1] == "/monitor_schema") {
            boost::property_tree::read_json("specification/monitor_schema.json", temp_json);
        } else if (words[1] == "/adaptation_schema") {
            boost::property_tree::read_json("specification/execute_schema.json", temp_json);
        } else {
            status_code = "404 Not Found";
            response_body = "";
        }

        // Get the JSON string if the request is valid
        if (status_code == "200 OK") {
            std::ostringstream oss;
            boost::property_tree::write_json(oss, temp_json);
            response_body = oss.str();
        }

    } else if (words[0] == "PUT") {
        response_type = "text/plain";

        if (words[1] == "/execute") {
            std::string request_body = lines.back();

            std::cout << request_body << endl;

            std::istringstream json_stream(request_body);
            boost::property_tree::ptree json_request;
            boost::property_tree::read_json(json_stream, json_request);

            boost::property_tree::ptree::const_iterator first_entry = json_request.begin();
            std::string first_key = first_entry->first;

            std::cout << first_key << endl;

            std::string arg = json_request.get<std::string>(first_key);

            response_body = commandHandlers[first_key](arg);
        } else {
            status_code = "404 Not Found";
        }
    } else {
        status_code = "405 Method Not Allowed";
    }

    // Construct the HTTP response
    std::string http_response = 
        "HTTP/1.1 " + status_code + "\r\n"
        "Content-Type: " + response_type + "\r\n"
        "Content-Length: " + std::to_string(response_body.length()) + "\r\n"
        "Accept-Ranges: bytes\r\n"
        "Connection: close\r\n"
        "\r\n" + response_body;

    // Send the HTTP response
    rtScheduler->sendBytes(http_response.c_str(), http_response.length());
}

std::string HTTPInterface::cmdSetServer(const std::string& arg) {
    if (arg == "Add") {
        return HTTPInterface::cmdAddServer(arg);
    } else if (arg == "Remove") {
        return HTTPInterface::cmdRemoveServer(arg);
    } else {
        return "\"error: invalid server argument\"";
    }
}

std::string HTTPInterface::cmdAddServer(const std::string& arg) {
    ExecutionManagerModBase* pExecMgr = check_and_cast<ExecutionManagerModBase*> (getParentModule()->getSubmodule("executionManager"));
    pExecMgr->addServer();

    return COMMAND_SUCCESS;
}

std::string HTTPInterface::cmdRemoveServer(const std::string& arg) {
    ExecutionManagerModBase* pExecMgr = check_and_cast<ExecutionManagerModBase*> (getParentModule()->getSubmodule("executionManager"));
    pExecMgr->removeServer();

    return COMMAND_SUCCESS;
}

std::string HTTPInterface::cmdSetDimmer(const std::string& arg) {
    if (arg == "") {
        return "\"error: missing dimmer argument\"";
    }

    double dimmer = atof(arg.c_str());
    ExecutionManagerModBase* pExecMgr = check_and_cast<ExecutionManagerModBase*> (getParentModule()->getSubmodule("executionManager"));
    pExecMgr->setBrownout(1 - dimmer);

    return COMMAND_SUCCESS;
}


std::string HTTPInterface::cmdGetDimmer(const std::string& arg) {
    ostringstream reply;
    double brownoutFactor = pModel->getBrownoutFactor();
    double dimmer = (1 - brownoutFactor);

    reply << dimmer;

    return reply.str();
}


std::string HTTPInterface::cmdGetServers(const std::string& arg) {
    ostringstream reply;
    reply << pModel->getServers();

    return reply.str();
}


std::string HTTPInterface::cmdGetActiveServers(const std::string& arg) {
    ostringstream reply;
    reply << pModel->getActiveServers();

    return reply.str();
}


std::string HTTPInterface::cmdGetMaxServers(const std::string& arg) {
    ostringstream reply;
    reply << pModel->getMaxServers();

    return reply.str();
}


std::string HTTPInterface::cmdGetUtilization(const std::string& arg) {
    if (arg == "") {
        return "\"error: missing server argument\"";
    }

    ostringstream reply;
    auto utilization = pProbe->getUtilization(arg);
    if (utilization < 0) {
        reply << "\"error: server \'" << arg << "\' does no exist\"";
    } else {
        reply << utilization;
    }

    return reply.str();
}

std::string HTTPInterface::cmdGetBasicResponseTime(const std::string& arg) {
    ostringstream reply;
    reply << pProbe->getBasicResponseTime();

    return reply.str();
}

std::string HTTPInterface::cmdGetBasicThroughput(const std::string& arg) {
    ostringstream reply;
    reply << pProbe->getBasicThroughput();

    return reply.str();
}

std::string HTTPInterface::cmdGetOptResponseTime(const std::string& arg) {
    ostringstream reply;
    reply << pProbe->getOptResponseTime();

    return reply.str();
}

std::string HTTPInterface::cmdGetOptThroughput(const std::string& arg) {
    ostringstream reply;
    reply << pProbe->getOptThroughput();

    return reply.str();
}

std::string HTTPInterface::cmdGetArrivalRate(const std::string& arg) {
    ostringstream reply;
    reply << pProbe->getArrivalRate();

    return reply.str();
}
