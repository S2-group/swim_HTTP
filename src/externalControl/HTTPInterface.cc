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
#include <managers/execution/ExecutionManagerMod.h>

Define_Module(HTTPInterface);

#define DEBUG_HTTP_INTERFACE 0

using namespace std;

namespace {
    const string UNKNOWN_COMMAND = "error: unknown command\n";
    const string COMMAND_SUCCESS = "OK\n";
    const string BAD_REQUEST = "400 Bad Request";
    const string MONITOR_SCHEMA_PATH = "specification/monitor_schema.json";
    const string UNKNOWN_ENDPOINT = "404 Not Found";
    const string METHOD_UNALLOW =  "405 Method Not Allowed";
    const string HTTP_OK = "200 OK";
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


    monitorHandlers["dimmer"] = std::bind(&Model::getBrownoutFactor, &Model);
//    commandHandlers["servers"] = std::bind(&HTTPInterface::cmdGetServers, this, std::placeholders::_1);
//    commandHandlers["active_servers"] = std::bind(&HTTPInterface::cmdGetActiveServers, this, std::placeholders::_1);
//    commandHandlers["max_servers"] = std::bind(&HTTPInterface::cmdGetMaxServers, this, std::placeholders::_1);
//    commandHandlers["utilization"] = std::bind(&HTTPInterface::cmdGetUtilization, this, std::placeholders::_1);
//    commandHandlers["basic_rt"] = std::bind(&HTTPInterface::cmdGetBasicResponseTime, this, std::placeholders::_1);
//    commandHandlers["basic_throughput"] = std::bind(&HTTPInterface::cmdGetBasicThroughput, this, std::placeholders::_1);
//    commandHandlers["opt_rt"] = std::bind(&HTTPInterface::cmdGetOptResponseTime, this, std::placeholders::_1);
//    commandHandlers["opt_throughput"] = std::bind(&HTTPInterface::cmdGetOptThroughput, this, std::placeholders::_1);
//    commandHandlers["arrival_rate"] = std::bind(&HTTPInterface::cmdGetArrivalRate, this, std::placeholders::_1);

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
    std::map<std::string, std::map<std::string, std::function<std::string(const std::string&)>>>::iterator method_it;
    method_it = HTTPAPI.find(http_rq_type);

    if (method_it == HTTPAPI.end())
    {
        HTTPInterface::sendResponse(METHOD_UNALLOW,"");
        return;
    }

    std::map<std::string, std::function<std::string(const std::string&)>>::iterator endpoint_it;
    HTTPAPI[http_rq_type].find(http_rq_endpoint);

    if (endpoint_it == HTTPAPI[http_rq_type].end())
    {
        HTTPInterface::sendResponse(UNKNOWN_ENDPOINT,"");
        return;
    }

    HTTPAPI[http_rq_type][http_rq_endpoint](http_rq_body);





    // Get the JSON string if request is valid
    if (status_code == "200 OK") {
        std::ostringstream oss;
        boost::property_tree::write_json(oss, temp_json);
        response_body = oss.str();
    }


    // Send the HTTP response
//    rtScheduler->sendBytes(http_response.c_str(), http_response.length());
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

    //monitor_mapping["utilization"] = std::bind(&HTTPInterface::addUtilization, this, std::placeholders::_1);
}

boost::property_tree::ptree HTTPInterface::allUtilization() {
    int max = std::stoi(HTTPInterface::cmdGetMaxServers(""));
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
void HTTPInterface::putInJSON (std::string json_key, T item, boost::property_tree::ptree& json_file) {
    json_file.put(json_key, item);
}

std::string HTTPInterface::epMonitor(const std::string& arg){
    HTTPInterface::updateMonitoring();
    for (const std::string& monitorable : monitorable_list) {
        json_file.put(monitorable, dimmer_factor);

        // Add key-value pairs to the JSON object
        //temp_json.put(monitorable, commandHandlers[monitorable](std::string()));
    }
    return "";

}
std::string HTTPInterface::epMonitorSchema(const std::string& arg){
    boost::property_tree::read_json(MONITOR_SCHEMA_PATH, temp_json);

}
std::string HTTPInterface::epExecuteSchema(const std::string& arg){
    boost::property_tree::read_json("specification/execute_schema.json", temp_json);

}
std::string HTTPInterface::epAdapOptions(const std::string& arg){
    boost::property_tree::read_json("specification/adaptation_options.json", temp_json);
}
std::string HTTPInterface::epAdapOptSchema(const std::string& arg)
{
    boost::property_tree::read_json("specification/adaptation_options_schema.json", temp_json);
}

std::string HTTPInterface::epExecute(const std::string& arg){
    std::string request_body = lines.back();

    std::istringstream json_stream(request_body);
    boost::property_tree::ptree json_request;
    boost::property_tree::read_json(json_stream, json_request);

    std::string servers_now, dimmer_now;
    //servers_now = HTTPInterface::cmdGetServers(std::string());
    //dimmer_now = HTTPInterface::cmdGetDimmer(std::string());

    try {
        std::string servers_request, dimmer_request, server_request_status, dimmer_request_status;
        servers_request = json_request.get<std::string>("server_number");
        dimmer_request = json_request.get<std::string>("dimmer_factor");

        if (servers_now != servers_request) {
            //server_request_status = HTTPInterface::cmdSetServers(servers_request);
        } else {
            server_request_status = "Number of servers already satisfied";
        }

        if (dimmer_now != dimmer_request) {
            //dimmer_request_status = HTTPInterface::cmdSetDimmer(dimmer_request);
        } else {
            dimmer_request_status = "Dimmer factor already satisfied";
        }

        temp_json.put("server_number", server_request_status);
        temp_json.put("dimmer_factor", dimmer_request_status);
    } catch (boost::property_tree::ptree_bad_path& e) {
        status_code = "400 Bad Request";
        temp_json.put("error", std::string("Missing key in request: ") + e.what());
    }

}



//std::string HTTPInterface::cmdSetServers(const std::string& arg) {
//    try {
//        int arg_int, val, max, diff;
//        arg_int = std::stoi(arg);
//        val = std::stoi(HTTPInterface::cmdGetServers(""));
//        max = std::stoi(HTTPInterface::cmdGetMaxServers(""));
//        diff = std::abs(arg_int - val);
//
//        if (arg_int > max) {
//            return "error: max servers exceeded";
//        }
//
//        for (int i = 0; i < diff; ++i) {
//            if (arg_int < val) {
//                HTTPInterface::cmdRemoveServer(arg);
//            } else if (arg_int > val && arg_int <= max) {
//                HTTPInterface::cmdAddServer(arg);
//            }
//        }
//
//        return COMMAND_SUCCESS;
//    }
//    catch (const std::invalid_argument& ia) {
//        return "error: invalid argument";
//    }
//    catch (const std::out_of_range& oor) {
//        return "error: out of range";
//    }
//}


//
//std::string HTTPInterface::cmdAddServer(const std::string& arg) {
//    ExecutionManagerModBase* pExecMgr = check_and_cast<ExecutionManagerModBase*> (getParentModule()->getSubmodule("executionManager"));
//    pExecMgr->addServer();
//
//    return COMMAND_SUCCESS;
//}
//
//std::string HTTPInterface::cmdRemoveServer(const std::string& arg) {
//    ExecutionManagerModBase* pExecMgr = check_and_cast<ExecutionManagerModBase*> (getParentModule()->getSubmodule("executionManager"));
//    pExecMgr->removeServer();
//
//    return COMMAND_SUCCESS;
//}
//
//std::string HTTPInterface::cmdSetDimmer(const std::string& arg) {
//    if (arg == "") {
//        return "\"error: missing dimmer argument\"";
//    }
//
//    double dimmer = atof(arg.c_str());
//    ExecutionManagerModBase* pExecMgr = check_and_cast<ExecutionManagerModBase*> (getParentModule()->getSubmodule("executionManager"));
//    pExecMgr->setBrownout(1 - dimmer);
//
//    return COMMAND_SUCCESS;
//}
//
//
//std::string HTTPInterface::cmdGetDimmer(const std::string& arg) {
//    ostringstream reply;
//    double brownoutFactor = pModel->getBrownoutFactor();
//    double dimmer = (1 - brownoutFactor);
//
//    reply << dimmer;
//
//    return reply.str();
//}
//
//
//std::string HTTPInterface::cmdGetServers(const std::string& arg) {
//    ostringstream reply;
//    reply << pModel->getServers();
//
//    return reply.str();
//}
//
//
//std::string HTTPInterface::cmdGetActiveServers(const std::string& arg) {
//    ostringstream reply;
//    reply << pModel->getActiveServers();
//
//    return reply.str();
//}
//
//
//std::string HTTPInterface::cmdGetMaxServers(const std::string& arg) {
//    ostringstream reply;
//    reply << pModel->getMaxServers();
//
//    return reply.str();
//}
//
//
//std::string HTTPInterface::cmdGetUtilization(const std::string& arg) {
//    if (arg == "") {
//        return "\"error: missing server argument\"";
//    }
//
//    ostringstream reply;
//    auto utilization = pProbe->getUtilization(arg);
//    if (utilization < 0) {
//        reply << "\"error: server \'" << arg << "\' does no exist\"";
//    } else {
//        reply << utilization;
//    }
//
//    return reply.str();
//}
//
//std::string HTTPInterface::cmdGetBasicResponseTime(const std::string& arg) {
//    ostringstream reply;
//    reply << pProbe->getBasicResponseTime();
//
//    return reply.str();
//}
//
//std::string HTTPInterface::cmdGetBasicThroughput(const std::string& arg) {
//    ostringstream reply;
//    reply << pProbe->getBasicThroughput();
//
//    return reply.str();
//}
//
//std::string HTTPInterface::cmdGetOptResponseTime(const std::string& arg) {
//    ostringstream reply;
//    reply << pProbe->getOptResponseTime();
//
//    return reply.str();
//}
//
//std::string HTTPInterface::cmdGetOptThroughput(const std::string& arg) {
//    ostringstream reply;
//    reply << pProbe->getOptThroughput();
//
//    return reply.str();
//}
//
//std::string HTTPInterface::cmdGetArrivalRate(const std::string& arg) {
//    ostringstream reply;
//    reply << pProbe->getArrivalRate();
//
//    return reply.str();
//}
