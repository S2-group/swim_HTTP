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
#include <sstream>
#include <boost/tokenizer.hpp>
#include <managers/execution/ExecutionManagerMod.h>
#include "HttpMsg_m.h"

Define_Module(HTTPInterface);

#define DEBUG_HTTP_INTERFACE 0

using namespace std;

namespace {
    const string UNKNOWN_COMMAND = "error: unknown command\n";
    const string COMMAND_SUCCESS = "OK\n";
}

HTTPInterface::HTTPInterface() {
    commandHandlers["/add_server"] = std::bind(&HTTPInterface::cmdAddServer, this, std::placeholders::_1);
    commandHandlers["/remove_server"] = std::bind(&HTTPInterface::cmdRemoveServer, this, std::placeholders::_1);
    commandHandlers["/set_dimmer"] = std::bind(&HTTPInterface::cmdSetDimmer, this, std::placeholders::_1);


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

void HTTPInterface::handleMessage(cMessage *msg)
{
    if (msg == rtEvent) {

        // get data from buffer
        string input = string(recvBuffer, numRecvBytes);
        numRecvBytes = 0;

        vector<string> words;
        int startIndex = 0, endIndex = 0;
        for (int i = 0; i <= input.size(); i++) {
            if (input[i] == ' ' || i == input.size()) {
                endIndex = i;
                string temp;
                temp.append(input, startIndex, endIndex - startIndex);
                words.push_back(temp);
                startIndex = endIndex + 1;
            }
        }

        std::string response_body = "";
        std::string status_code = "200 OK";
        std::string response_type;
        if (words[0] == "GET") {
            response_body += "{";
            response_type = "application/json";
            if (words[1] == "/monitor") {
                for (std::string monitorable : monitor) {
                    std::string value = commandHandlers[monitorable](std::vector<string>());
                    response_body += "\"" + monitorable + "\": " + value;

                    if (monitorable != monitor.back()) {
                        response_body += ", ";
                    }
                }
                response_body += "}";

            } else if (words[1] == "/monitor_schema") {

            } else if (words[1] == "/adaptation_schema") {
                
            } else {
                status_code = "404 Not Found";
                response_body = "";
            }
        } else if (words[0] == "PUT") {
            response_type = "text/plain";

            bool found = false;
            for (int i = 0; i < adaptations.size(); ++i) {
                if (adaptations[i] == words[1]) {
                    found = true;
                    break;
                }
            }

            if (found) {
                response_body = commandHandlers[words[1]](std::vector<std::string>());
            } else {
                status_code = "404 Not Found";
            }

        } else {
            status_code = "405 Method Not Allowed";
            response_body = "";
        }

        std::string http_response = "HTTP/1.1 " + status_code + "\r\n"
                               "Content-Type: " + response_type + "\r\n"
                               "Content-Length: " + std::to_string(response_body.length()) + "\r\n"
                               "Accept-Ranges: bytes\r\n"
                               "Connection: close\r\n"
                               "\r\n" + response_body;

        rtScheduler->sendBytes(http_response.c_str(), http_response.length());

        char recvBuffer[BUFFER_SIZE];
    }
}

std::string HTTPInterface::cmdAddServer(const std::vector<std::string>& args) {
    ExecutionManagerModBase* pExecMgr = check_and_cast<ExecutionManagerModBase*> (getParentModule()->getSubmodule("executionManager"));
    pExecMgr->addServer();

    return COMMAND_SUCCESS;
}

std::string HTTPInterface::cmdRemoveServer(const std::vector<std::string>& args) {
    ExecutionManagerModBase* pExecMgr = check_and_cast<ExecutionManagerModBase*> (getParentModule()->getSubmodule("executionManager"));
    pExecMgr->removeServer();

    return COMMAND_SUCCESS;
}

std::string HTTPInterface::cmdSetDimmer(const std::vector<std::string>& args) {
    if (args.size() == 0) {
        return "\"error: missing dimmer argument\"";
    }

    double dimmer = atof(args[0].c_str());
    ExecutionManagerModBase* pExecMgr = check_and_cast<ExecutionManagerModBase*> (getParentModule()->getSubmodule("executionManager"));
    pExecMgr->setBrownout(1 - dimmer);

    return COMMAND_SUCCESS;
}


std::string HTTPInterface::cmdGetDimmer(const std::vector<std::string>& args) {
    ostringstream reply;
    double brownoutFactor = pModel->getBrownoutFactor();
    double dimmer = (1 - brownoutFactor);

    reply << dimmer;

    return reply.str();
}


std::string HTTPInterface::cmdGetServers(const std::vector<std::string>& args) {
    ostringstream reply;
    reply << pModel->getServers();

    return reply.str();
}


std::string HTTPInterface::cmdGetActiveServers(const std::vector<std::string>& args) {
    ostringstream reply;
    reply << pModel->getActiveServers();

    return reply.str();
}


std::string HTTPInterface::cmdGetMaxServers(const std::vector<std::string>& args) {
    ostringstream reply;
    reply << pModel->getMaxServers();

    return reply.str();
}


std::string HTTPInterface::cmdGetUtilization(const std::vector<std::string>& args) {
    if (args.size() == 0) {
        return "\"error: missing server argument\"";
    }

    ostringstream reply;
    auto utilization = pProbe->getUtilization(args[0]);
    if (utilization < 0) {
        reply << "\"error: server \'" << args[0] << "\' does no exist\"";
    } else {
        reply << utilization;
    }

    return reply.str();
}

std::string HTTPInterface::cmdGetBasicResponseTime(
        const std::vector<std::string>& args) {
    ostringstream reply;
    reply << pProbe->getBasicResponseTime();

    return reply.str();
}

std::string HTTPInterface::cmdGetBasicThroughput(
        const std::vector<std::string>& args) {
    ostringstream reply;
    reply << pProbe->getBasicThroughput();

    return reply.str();
}

std::string HTTPInterface::cmdGetOptResponseTime(
        const std::vector<std::string>& args) {
    ostringstream reply;
    reply << pProbe->getOptResponseTime();

    return reply.str();
}

std::string HTTPInterface::cmdGetOptThroughput(
        const std::vector<std::string>& args) {
    ostringstream reply;
    reply << pProbe->getOptThroughput();

    return reply.str();
}

std::string HTTPInterface::cmdGetArrivalRate(
        const std::vector<std::string>& args) {
    ostringstream reply;
    reply << pProbe->getArrivalRate();

    return reply.str();
}
