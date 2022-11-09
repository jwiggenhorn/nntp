#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <deque>
#include <vector>
#include <fstream>
#include <iostream>
#include <regex>
#include <map>
#include <unordered_set>
#include <ctime>
#include <signal.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#define MAX_DATA_SIZE 1048576 // buffer size for messages

using std::cin;
using std::cout;
using std::endl;
using std::string;
using std::to_string;
using std::stringstream;
using std::ifstream;
using std::deque;
using std::unordered_set;
using std::vector;
using std::regex;
using std::regex_replace;

// function declarations
void read_config(string filename, string &port);
void read_config(string filename, string &ip, string &port);
void sigchld_handler(int s);
string to_upper(string input);
string remove_char(string input, char char_to_remove);
string trim(string input);
deque<string> split(string &input, char delimiter);
string escape_special_chars(string input);
bool is_int(const string s);
bool match(string name, string wildmat);

// server config
void read_config(string filename, string &port) {
    string line;
    ifstream fs(filename);
    if (fs.is_open()) {
        while (getline(fs, line)) {
            line = line.substr(0, line.find("#")); // strip comments
            if (line.find("NNTP_PORT") != string::npos) {
                port = trim(line.substr(line.find("=") + 1, line.length()));
            }
        }    
        fs.close();
    }
    if (port.empty()) {
        cout << "Invalid configuration file." << endl;
        exit(0);
    }
}

// client config
void read_config(string filename, string &ip, string &port) {
    string line;
    std::ifstream fs(filename);
    if (fs.is_open()) {
        while (getline(fs, line)) {
            line = line.substr(0, line.find("#")); // strip comments
            if (line.find("SERVER_IP") != string::npos) {
                ip = trim(line.substr(line.find("=") + 1, line.length()));
            } 
            else if (line.find("SERVER_PORT") != string::npos) {
                port = trim(line.substr(line.find("=") + 1, line.length()));
            }
        }    
        fs.close();
    }
    if (ip.empty() || port.empty()) {
        cout << "Invalid configuration file." << endl;
        exit(0);
    }
}

string to_upper(string input) {
    string output = "";
    for (auto c : input) {
        output += std::toupper(c);
    }
    return output;
}

string remove_char(string input, char char_to_remove) {
    string output = input;
    output.erase(remove(output.begin(), output.end(), char_to_remove), output.end()); 
    return output;
}

// remove leading and trailing spaces from input
string trim(string input) {
    string output = input;
    string space = " \t\n\r\f\v";
    output.erase(0, output.find_first_not_of(space));
    output.erase(output.find_last_not_of(space) + 1);
    return output;
}

deque<string> split(string &input, char delimiter) {
    deque<string> result;
    stringstream ss(input);
    string token;

    while (getline(ss, token, delimiter)) {
        result.push_back(token);
    }

    return result;
}

string escape_special_chars(string input) {
    string special_chars = "-^.$|(){}+/#";
    string output = "";
    for (auto c : input) {
        if (special_chars.find(c) != string::npos) {
            output += "\\" + c;
        } else {
            output += c;
        }
    }
    return output;
}

bool is_int(const string input) {
    return input.find_first_not_of("0123456789") == string::npos;
}

bool match(string name, string wildmat) {
    string regex_string = escape_special_chars(trim(wildmat));
    regex_string = regex_replace(regex_string, regex("\\?"), ".");
    regex_string = regex_replace(regex_string, regex("\\*"), ".*");
    bool match = false;
    bool negate = false;
    deque<string> patterns = split(regex_string, ',');
    for (auto pattern : patterns) {
        if (regex_match(name, regex(remove_char(pattern, '!')))) {
            if (pattern[0] == '!') {
                negate = true;
                match = false;
            } else {
                if (!negate) {
                    match = true;
                }
                negate = false;
            }
        }
    }
    return match;
}