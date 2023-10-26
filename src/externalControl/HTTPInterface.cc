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
#include <regex>
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

std::string removeQuotesAroundNumbers(const std::string& input) {
    std::regex regex(R"delim("(-?\d+(\.\d+)?)")delim"); // matches quoted numbers, including decimals
    return std::regex_replace(input, regex, "$1");
}

HTTPInterface::HTTPInterface() {
    monitor_mapping["dimmer"] = std::bind(&HTTPInterface::addDimmer, this, std::placeholders::_1);
    monitor_mapping["servers"] = std::bind(&HTTPInterface::addServers, this, std::placeholders::_1);
    monitor_mapping["active_servers"] = std::bind(&HTTPInterface::addActiveServers, this, std::placeholders::_1);
    monitor_mapping["max_servers"] = std::bind(&HTTPInterface::addMaxServers, this, std::placeholders::_1);
    monitor_mapping["utilization"] = std::bind(&HTTPInterface::addUtilization, this, std::placeholders::_1);
    monitor_mapping["basic_rt"] = std::bind(&HTTPInterface::addBasicResponseTime, this, std::placeholders::_1);
    monitor_mapping["basic_throughput"] = std::bind(&HTTPInterface::addBasicThroughput, this, std::placeholders::_1);
    monitor_mapping["opt_rt"] = std::bind(&HTTPInterface::addOptResponseTime, this, std::placeholders::_1);
    monitor_mapping["opt_throughput"] = std::bind(&HTTPInterface::addOptThroughput, this, std::placeholders::_1);
    monitor_mapping["arrival_rate"] = std::bind(&HTTPInterface::addArrivalRate, this, std::placeholders::_1);

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
                monitor_mapping[monitorable](temp_json);
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
    } else if (words[0] == "PUT") {
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
        response_body = removeQuotesAroundNumbers(oss.str());
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
    ExecutionManagerModBase* pExecMgr = check_and_cast<ExecutionManagerModBase*> (getParentModule()->getSubmodule("executionManager"));

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
                pExecMgr->removeServer();
            } else if (arg_int > val && arg_int <= max) { 
                pExecMgr->addServer();
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

std::string HTTPInterface::cmdGetMaxServers(const std::string& arg) {
    ostringstream reply;
    reply << pModel->getMaxServers();

    return reply.str();
}

void HTTPInterface::addDimmer(boost::property_tree::ptree& json) {
    double brownoutFactor = pModel->getBrownoutFactor();
    double dimmer = (1 - brownoutFactor);

    json.put("dimmer", dimmer);
}

void HTTPInterface::addServers(boost::property_tree::ptree& json) {
    int servers = pModel->getServers();

    json.put("servers", servers);
}

void HTTPInterface::addActiveServers(boost::property_tree::ptree& json) {
    int active_servers = pModel->getActiveServers();

    json.put("active_servers", active_servers);
}

void HTTPInterface::addMaxServers(boost::property_tree::ptree& json) {
    int max_servers = pModel->getMaxServers();

    json.put("max_servers", max_servers);
}

void HTTPInterface::addUtilization(boost::property_tree::ptree& json) {
    int max = std::stoi(HTTPInterface::cmdGetMaxServers(""));
    boost::property_tree::ptree server_array_ptree;

    for(int i = 1; i <= max; i++) {
        boost::property_tree::ptree server;
        std::string server_name = "server" + std::to_string(i);
        auto utilization = pProbe->getUtilization(server_name);

        if (utilization < 0) {
            utilization = 0;
        }

        server.put("server_name", server_name);
        server.put("utilization_value", utilization);

        // Add each server ptree to the main ptree using push_back
        server_array_ptree.push_back(std::make_pair("", server));
    }

    json.put_child("utilization", server_array_ptree);
}

void HTTPInterface::addBasicResponseTime(boost::property_tree::ptree& json) {
     double basic_rt = pProbe->getBasicResponseTime();

    json.put("basic_rt", basic_rt);
}

void HTTPInterface::addBasicThroughput(boost::property_tree::ptree& json) {
    double basic_throughput = pProbe->getBasicThroughput();

    json.put("basic_throughput", basic_throughput);
}

void HTTPInterface::addOptResponseTime(boost::property_tree::ptree& json) {
    double opt_rt = pProbe->getOptResponseTime();

    json.put("opt_rt", opt_rt);
}

void HTTPInterface::addOptThroughput(boost::property_tree::ptree& json) {
    double opt_throughput = pProbe->getOptThroughput();

    json.put("opt_throughput", opt_throughput);
}

void HTTPInterface::addArrivalRate(boost::property_tree::ptree& json) {
    double arrival_rate = pProbe->getArrivalRate();

    json.put("arrival_rate", arrival_rate);
}
