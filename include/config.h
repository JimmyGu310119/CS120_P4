#ifndef CONFIG_H
#define CONFIG_H

#include "utils.h"
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

class Config {
public:
    enum Node { NODE1 = 1, NODE2 = 2 };
    // Project 4 核心协议类型
    enum Type { 
        DNS_REQ = 20, 
        DNS_RSP = 21, 
        TCP_SYN = 30, 
        TCP_ACK = 31, 
        TCP_DATA = 32,
        HTTP_REQ = 40,
        HTTP_RSP = 41
    };

    std::string ip;
    int port;
    Node node;
};

class GlobalConfig {
public:
    GlobalConfig();
    Config get(Config::Node node);
private:
    std::vector<Config> _config;
};

#endif