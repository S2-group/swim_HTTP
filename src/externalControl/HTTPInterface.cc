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
#include <cmath>
#include <sstream>
#include <boost/tokenizer.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
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
    // Reset buffer
    char recvBuffer[BUFFER_SIZE];

    std::vector<std::string> words;
    std::istringstream iss(input);
    for (std::string word; iss >> word;) { words.push_back(word); }

    std::stringstream ss(input);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(ss, line, '\n')) { lines.push_back(line); }

    std::string response_body = "";
    std::string status_code = "200 OK";
    boost::property_tree::ptree temp_json;

    if (words.empty()) {
        // Handle the case where there are no words
        status_code = "400 Bad Request";
    } else if (words[0] == "GET") {
        if (words[1] == "/monitor") {
            for (const std::string& monitorable : monitor) {
                // Add key-value pairs to the JSON object
                temp_json.put(monitorable, commandHandlers[monitorable](std::string()));
            }
        } else if (words[1] == "/monitor_schema") {
            boost::property_tree::read_json("specification/monitor_schema.json", temp_json);
        } else if (words[1] == "/execute_schema") {
            boost::property_tree::read_json("specification/execute_schema.json", temp_json);
        } else if (words[1] == "/adaptation_options") {
            boost::property_tree::read_json("specification/adaptation_options.json", temp_json);
        } else if (words[1] == "/adaptation_options_schema") {
            boost::property_tree::read_json("specification/adaptation_options_schema.json", temp_json);
        } else {
            status_code = "404 Not Found";
        }
    } else if (words[0] == "POST") {
        if (words[1] == "/execute") {
            std::string request_body = lines.back();

            std::istringstream json_stream(request_body);
            boost::property_tree::ptree json_request;
            boost::property_tree::read_json(json_stream, json_request);

            std::string servers_now, dimmer_now;
            servers_now = HTTPInterface::cmdGetServers(std::string());
            dimmer_now = HTTPInterface::cmdGetDimmer(std::string());

            try {
                std::string servers_request, dimmer_request, server_request_status, dimmer_request_status;
                servers_request = json_request.get<std::string>("server_number");
                dimmer_request = json_request.get<std::string>("dimmer_factor");

                if (servers_now != servers_request) {
                    server_request_status = HTTPInterface::cmdSetServers(servers_request);
                } else {
                    server_request_status = "Number of servers already satisfied";
                }

                if (dimmer_now != dimmer_request) {
                    dimmer_request_status = HTTPInterface::cmdSetDimmer(dimmer_request);
                } else {
                    dimmer_request_status = "Dimmer factor already satisfied";
                }

                temp_json.put("server_number", server_request_status);
                temp_json.put("dimmer_factor", dimmer_request_status);
            } catch (boost::property_tree::ptree_bad_path& e) {
                status_code = "400 Bad Request"; 
                temp_json.put("error", std::string("Missing key in request: ") + e.what());
            }
        } else {
            status_code = "404 Not Found";
        }
    } else {
        status_code = "405 Method Not Allowed";
    }

    // Get the JSON string if request is valid
    if (status_code == "200 OK") {
        std::ostringstream oss;
        boost::property_tree::write_json(oss, temp_json);
        response_body = oss.str();
    }

    // Construct the HTTP response
    std::string http_response = 
        "HTTP/1.1 " + status_code + "\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(response_body.length()) + "\r\n"
        "Accept-Ranges: bytes\r\n"
        "Connection: close\r\n"
        "\r\n" + response_body;

    // Send the HTTP response
    rtScheduler->sendBytes(http_response.c_str(), http_response.length());
}

std::string HTTPInterface::cmdSetServers(const std::string& arg) {
    try {
        int arg_int, val, max, diff;
        arg_int = std::stoi(arg);
        val = std::stoi(HTTPInterface::cmdGetServers(""));
        max = std::stoi(HTTPInterface::cmdGetMaxServers(""));
        diff = std::abs(arg_int - val);

        if (arg_int > max) {
            return "error: max servers exceeded";
        }

        for (int i = 0; i < diff; ++i) {
            if (arg_int < val) {
                HTTPInterface::cmdRemoveServer(arg);
            } else if (arg_int > val && arg_int <= max) { 
                HTTPInterface::cmdAddServer(arg);
            }
        }

        return COMMAND_SUCCESS;
    }
    catch (const std::invalid_argument& ia) {
        return "error: invalid argument";
    }
    catch (const std::out_of_range& oor) {
        return "error: out of range";
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
