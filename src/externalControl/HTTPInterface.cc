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
#include <managers/execution/ExecutionManagerMod.h>

Define_Module(HTTPInterface);

#define DEBUG_HTTP_INTERFACE 0

using namespace std;

namespace {
    const string UNKNOWN_COMMAND = "error: unknown command\n";
    const string COMMAND_SUCCESS = "OK\n";
    const string BAD_REQUEST = "400 Bad Request";
    const string ADAPTATION_OPTIONS_PATH = "specification/adaptation_options.json";
    const string MONITOR_SCHEMA_PATH = "specification/monitor_schema.json";
    const string EXECUTE_SCHEMA_PATH = "specification/execute_schema.json";
    const string ADAPTATION_OPTIONS_SCHEMA_PATH = "specification/adaptation_options_schema.json";
    const string UNKNOWN_ENDPOINT = "404 Not Found";
    const string METHOD_UNALLOW =  "405 Method Not Allowed";
    const string HTTP_OK = "200 OK";
}

std::string removeQuotesAroundNumbers(boost::property_tree::ptree& json_file) {
    std::ostringstream oss;
    boost::property_tree::write_json(oss, json_file);
    std::regex regex(R"delim("(-?\d+(\.\d+)?)")delim"); // matches quoted numbers, including decimals

    return std::regex_replace(oss.str(), regex, "$1");
}

HTTPInterface::HTTPInterface() {
    // GET Requests
    endpointGETHandlers["/monitor"] = std::bind(&HTTPInterface::epMonitor, this, std::placeholders::_1);
    endpointGETHandlers["/monitor_schema"] = std::bind(&HTTPInterface::epMonitorSchema, this, std::placeholders::_1);
    endpointGETHandlers["/execute_schema"] = std::bind(&HTTPInterface::epExecuteSchema, this, std::placeholders::_1);
    endpointGETHandlers["/adaptation_options"] = std::bind(&HTTPInterface::epAdapOptions, this, std::placeholders::_1);
    endpointGETHandlers["/adaptation_options_schema"] = std::bind(&HTTPInterface::epAdapOptSchema, this, std::placeholders::_1);

    // PUT Request
    endpointPUTHandlers["/execute"] = std::bind(&HTTPInterface::epExecute, this, std::placeholders::_1);

    HTTPAPI["GET"] = endpointGETHandlers;
    HTTPAPI["PUT"] = endpointPUTHandlers;

    integerMonitorable = {
            {"servers", &servers},
            {"active_servers", &active_servers},
            {"max_servers", &max_servers}};

    doubleMonitorable = {
            {"dimmer_factor", &dimmer_factor},
            {"basic_rt", &basic_rt},
            {"basic_throughput", &basic_throughput},
            {"opt_throughput", &opt_throughput},
            {"opt_rt", &opt_rt},
            {"arrival_rate", &arrival_rate}};

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

bool HTTPInterface::parseMessage() {
    // Get data from the buffer
    std::string input(recvBuffer, numRecvBytes);
    numRecvBytes = 0;
    // Reset buffer
    char recvBuffer[BUFFER_SIZE];

    std::vector<std::string> http_request;
    std::istringstream iss(input);
    for (std::string word; iss >> word;) { http_request.push_back(word); }

    if (http_request.empty()) {
         // Handle the case where there are no words
         return false;
     }

    std::stringstream ss(input);

    std::string line;
    while (std::getline(ss, line, '\n')) { lines.push_back(line); }


    http_rq_type = http_request[0];
    http_rq_endpoint = http_request[1];

    if(lines.size() > 0){
        http_rq_body = lines.back();

    }
    else {
        http_rq_body = "";
    }

    return true;


}

void HTTPInterface::sendResponse(const std::string& status_code, const std::string& response_body) {
    std::string http_response =
        "HTTP/1.1 " + status_code + "\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(response_body.length()) + "\r\n"
        "Accept-Ranges: bytes\r\n"
        "Connection: close\r\n"
        "\r\n" + response_body;

    rtScheduler->sendBytes(http_response.c_str(), http_response.length());


}
void HTTPInterface::handleMessage(cMessage *msg) {

    response_json = boost::property_tree::ptree();

    if (msg != rtEvent) {
        // Handle the message only if it's the expected event
        return;
    }

    bool valid_message = HTTPInterface::parseMessage();

    if(!valid_message)
    {
        HTTPInterface::sendResponse(BAD_REQUEST,"");
        return;
    }
    std::map<std::string, std::map<std::string, std::function<bool(const std::string&)>>>::iterator method_it;
    method_it = HTTPAPI.find(http_rq_type);

    if (method_it == HTTPAPI.end())
    {
        HTTPInterface::sendResponse(METHOD_UNALLOW,"");
        return;
    }

    std::map<std::string, std::function<bool(const std::string&)>>::iterator endpoint_it;
    HTTPAPI[http_rq_type].find(http_rq_endpoint);

    if (endpoint_it == HTTPAPI[http_rq_type].end())
    {
        HTTPInterface::sendResponse(UNKNOWN_ENDPOINT,"");
        return;
    }

    bool isSuccess = HTTPAPI[http_rq_type][http_rq_endpoint](http_rq_body);


    if(isSuccess){
        response_body = removeQuotesAroundNumbers(response_json);

        HTTPInterface::sendResponse(HTTP_OK,response_body);
    }
}

void HTTPInterface::updateMonitoring(){
    dimmer_factor = pModel->getDimmerFactor();
    servers =  pModel->getServers();
    active_servers = pModel->getActiveServers();
    basic_rt = pProbe->getBasicResponseTime();
    max_servers = pModel->getMaxServers();
    basic_throughput = pProbe->getBasicThroughput();
    opt_throughput = pProbe->getOptThroughput();
    opt_rt = pProbe->getOptResponseTime();
    arrival_rate = pProbe->getArrivalRate();
    utilization = HTTPInterface::allUtilization();
}

boost::property_tree::ptree HTTPInterface::allUtilization() {
    int max = pModel->getMaxServers();
    boost::property_tree::ptree server_array_ptree;

    for(int i = 1; i <= max; i++) {
        boost::property_tree::ptree server;
        std::string server_name = "server" + std::to_string(i);
        double utilization = pProbe->getUtilization(server_name);

        if (utilization < 0) {
            utilization = 0;
        }

        server.put("server_name", server_name);
        server.put("utilization_value", utilization);

        // Add each server ptree to the main ptree using push_back
        server_array_ptree.push_back(std::make_pair("", server));
    }
    return server_array_ptree;
}

template <class T>
void HTTPInterface::putInJSON (boost::property_tree::ptree& json_file, std::map<std::string, T*>& some_map) {
    for (auto const& map_entry : some_map)
    {
        json_file.put(map_entry.first, *map_entry.second);
    }
}

bool HTTPInterface::epMonitor(const std::string& arg){
    HTTPInterface::updateMonitoring();

    HTTPInterface::putInJSON<int>(response_json, integerMonitorable);
    HTTPInterface::putInJSON<double>(response_json, doubleMonitorable);

    response_json.put_child("utilization", utilization);

    return true;
}
bool HTTPInterface::epMonitorSchema(const std::string& arg){
    boost::property_tree::read_json(MONITOR_SCHEMA_PATH, response_json);

    return true;

}
bool HTTPInterface::epExecuteSchema(const std::string& arg){
    boost::property_tree::read_json(EXECUTE_SCHEMA_PATH, response_json);

    return true;

}
bool HTTPInterface::epAdapOptions(const std::string& arg){
    boost::property_tree::read_json(ADAPTATION_OPTIONS_PATH, response_json);
    return true;
}
bool HTTPInterface::epAdapOptSchema(const std::string& arg)
{
    boost::property_tree::read_json(ADAPTATION_OPTIONS_SCHEMA_PATH, response_json);
    return true;
}

bool HTTPInterface::epExecute(const std::string& arg){
    std::istringstream json_stream(http_rq_body);
    boost::property_tree::ptree json_request;
    boost::property_tree::read_json(json_stream, json_request);

    std::string servers_now, dimmer_now;
    servers_now = pModel->getActiveServers();
    dimmer_now = pModel->getDimmerFactor();

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

        response_json.put("server_number", server_request_status);
        response_json.put("dimmer_factor", dimmer_request_status);
    } catch (boost::property_tree::ptree_bad_path& e) {
        response_json.put("error", std::string("Missing key in request: ") + e.what());
        response_body = removeQuotesAroundNumbers(response_json);
        HTTPInterface::sendResponse(BAD_REQUEST,response_body);

        return false;
    }

    return true;

}



std::string HTTPInterface::cmdSetServers(const std::string& arg) {
    ExecutionManagerModBase* pExecMgr = check_and_cast<ExecutionManagerModBase*> (getParentModule()->getSubmodule("executionManager"));
    try {
        int arg_int, val, max, diff;
        arg_int = std::stoi(arg);
        val = pModel->getServers();
        max = pModel->getMaxServers();
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
