#include "utils.h" // contains utilities, some shared with client.cpp
#define BACKLOG 10 // for listen()

// NNTP commands
string get_group(deque<string> &args);
string get_listgroup(deque<string> &args);
string get_next_article(deque<string> &args);
string get_article(deque<string> &args);
string get_header(deque<string> &args);
string get_list(deque<string> &args);
string get_list_active(deque<string> &args);
string get_list_newsgroups(deque<string> &args);
string get_list_headers(deque<string> &args);
string get_date(deque<string> &args);
string get_capabilities(deque<string> &args);
string get_help(deque<string> &args);
string get_quit(deque<string> &args);

// command helper functions
string count_file_bytes(string path);                   // HDR
string count_file_body_lines(string path);              // HDR
string get_article_path(string message_id);             // HDR and ARTICLE
unordered_set<string> check_all_headers();              // HDR and LIST HEADERS
string get_newsgroup_name(string requested_newsgroup);  // GROUP and LISTGROUP

// server utils
void handle_client(int socket_fd);
void init_command_map();

// globals
enum command {
    undefined, group, listgroup, next, article, hdr,
    list, date, capabilities, help, quit
};
static std::map<string, command> command_map;
string current_group;
int current_article;

int main(int argc, char const *argv[]) {
    string nntp_port;
    if (argc == 2) {
        read_config(argv[1], nntp_port);
    } else {
        cout << "Please pass in a server.conf when running this program,\n";
        cout << "i.e., ./server server.conf" << endl;
        return 0;
    }
    init_command_map(); // initialize map used for switching on strings

    int socket_fd, new_fd;
    struct addrinfo hints, *client_info;
    struct sockaddr_storage client_addr;
    socklen_t sin_size;
    struct sigaction sa;
    const int opt_val = 1;
    char client_ip[INET_ADDRSTRLEN];

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_PASSIVE; 
    
    if (getaddrinfo(NULL, nntp_port.c_str(), &hints, &client_info) != 0) {
        perror("getaddrinfo");
        return 1;
    }
    socket_fd = socket(client_info->ai_family, client_info->ai_socktype, client_info->ai_protocol);
    if (socket_fd == -1) {
        perror("socket");
    }
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val)) == -1) {
        perror("setsockopt");
        exit(1);
    }
    if (bind(socket_fd, client_info->ai_addr, client_info->ai_addrlen) == -1) {
        close(socket_fd);
        perror("bind");
    }
    if (client_info == NULL) {
        fprintf(stderr, "Failed to bind\n");
        exit(1);
    }
    freeaddrinfo(client_info); 
    if (listen(socket_fd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    cout << "Waiting for connections..." << endl;
    while (true) {
        sin_size = sizeof(client_addr);
        new_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(client_addr.ss_family, &((struct sockaddr_in *)&client_addr)->sin_addr, client_ip, sizeof(client_ip));
        cout << "Got connection from " << client_ip << endl;

        if (!fork()) {
            close(socket_fd);

            string greeting = "201 Service available, posting prohibited";
            if (send(new_fd, greeting.c_str(), greeting.length(), 0) == -1) {
                perror("send");
            }
            cout << "Sent greeting to client" << endl;

            handle_client(new_fd);

            close(new_fd);
            exit(0);
        }
        close(new_fd);
    }
    return 0;
}

void handle_client(int socket_fd) {
    int num_bytes; 
    char buffer[MAX_DATA_SIZE];

    while (true) {
        if ((num_bytes = recv(socket_fd, buffer, MAX_DATA_SIZE - 1, 0)) == -1) {
            perror("recv");
            exit(1);
        }
        if (num_bytes == 0) {
            cout << "Client has closed the connection" << endl;
            break;
        }
        buffer[num_bytes] = '\0';

        // parse client request
        string cleaned_message = trim(buffer);
        deque<string> request = split(cleaned_message, ' ');
        string command = to_upper(request.front());
        cout << "Received command '" << command << "'" << endl;
        if (request.size() > 0) {
            request.pop_front(); // remove command, leaving just args
        }
        string response;
        switch (command_map[command]) {
            case group:
                response = get_group(request);
                break;
            case listgroup:
                response = get_listgroup(request);
                break;
            case next:
                response = get_next_article(request);
                break;
            case article:
                response = get_article(request);
                break;
            case hdr:
                response = get_header(request);
                break;
            case list:
                response = get_list(request);
                break;
            case date:
                response = get_date(request);
                break;
            case capabilities:
                response = get_capabilities(request);
                break;
            case help:
                response = get_help(request);
                break;
            case quit:
                response = get_quit(request);
                if (send(socket_fd, response.c_str(), response.length(), 0) == -1) {
                    perror("send");
                }
                return;
                break;
            default:
                response = "500 Unknown command";
                break;
        }

        if (send(socket_fd, response.c_str(), response.length(), 0) == -1) {
            perror("send");
        }
        cout << "Returned '" << response.substr(0, response.find("\n")) << "'" << endl;
    }
}

string get_group(deque<string> &args) {
    if (args.empty() || args.size() > 1) {
        return "501 Syntax error";
    }
    string selected_group = args.front();

    stringstream output;
    string group_dir_name = get_newsgroup_name(selected_group);
    if (!group_dir_name.empty()) {
        output << "211 ";
        current_group = group_dir_name;
        int num_articles = 0;
        int min = 0, max = 0;
        string path = "db/" + group_dir_name;
        DIR *dir = opendir(path.c_str());
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            string filename = entry->d_name;
            if (filename.find(".txt") != string::npos) {
                int watermark = stoi(filename.substr(0, filename.find(".")));
                num_articles++;
                min = (watermark < min || min == 0)? watermark: min;
                max = (watermark > max)? watermark: max;
            }
        }
        closedir(dir); 
        current_article = min;
        output << num_articles << " " << min << " " << max << " " << group_dir_name;
    } else {
        output << "411 " << selected_group << " is unknown";
    }
    return output.str();
}

string get_listgroup(deque<string> &args) {
    if (args.size() > 2) {
        return "501 Syntax error";
    }
    if (args.size() == 0 && current_group.empty()) {
        return "412 No newsgroup selected";
    }
    string selected_group = args.empty() ? current_group : args.front();
    string range = args.size() == 2 ? args.back() : ""; 
    int range_low = 1;
    int range_high = -1;
    if (!range.empty()) {
        try {
            if (range.find("-") == string::npos) { // single article
                range_low = stoi(range);
                range_high = stoi(range);
            } else {
                deque<string> range_tokens = split(range, '-');
                range_low = stoi(range_tokens.front());
                range_high = range_tokens.size() == 2 ? stoi(range_tokens.back()) : -1;
            }
        } catch (...) {
            return "501 Syntax error";
        }
    }

    stringstream output;
    string group_dir_name = get_newsgroup_name(selected_group);
    if (!group_dir_name.empty()) {
        output << "211 ";
        current_group = group_dir_name;
        vector<int> articles;
        int num_articles = 0;
        int min = 0, max = 0;
        string path = "db/" + group_dir_name;
        DIR *dir = opendir(path.c_str());
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            string filename = entry->d_name;
            if (filename.find(".txt") != string::npos) {
                int watermark = stoi(filename.substr(0, filename.find(".")));
                if ((range_high != -1 && watermark >= range_low && watermark <= range_high) ||
                    (range_high == -1 && watermark >= range_low)) {
                    articles.push_back(watermark);
                }
                num_articles++;
                min = (watermark < min || min == 0) ? watermark : min;
                max = (watermark > max) ? watermark : max;
            }
        }
        closedir(dir); 
        current_article = min;
        output << num_articles << " " << min << " " << max << " " << group_dir_name << " list follows\n";
        sort(articles.begin(), articles.end());
        for (auto a : articles) {
            output << a << "\n";
        }
        output << ".";
    } else {
        output << "411 " << selected_group << " is unknown";
    }
    return output.str();
}

string get_next_article(deque<string> &args) {
    if (!args.empty()) {
        return "501 Syntax error";
    }
    if (current_group.empty()) {
        return "412 No newsgroup selected";
    }

    string path = "db/" + current_group;
    DIR *dir = opendir(path.c_str());
    struct dirent *entry;
    vector<int> articles;
    while ((entry = readdir(dir)) != NULL) {
        string filename = entry->d_name;
        if (filename.find(".txt") != string::npos) {
            int watermark = stoi(filename.substr(0, filename.find(".")));
            articles.push_back(watermark);
        }
    }
    closedir(dir);
    if (articles.size() == 0) {
        return "420 No current article selected";
    }
    sort(articles.begin(), articles.end());
    int current_article_index = find(articles.begin(), articles.end(), current_article) - articles.begin();
    if (current_article_index + 1 >= articles.size()) {
        return "421 No next article to retrieve";
    }
    current_article = articles.at(current_article_index + 1);

    string message_id;
    string line;
    string filename = to_string(current_article) + ".txt";
    ifstream fs(path + "/" + filename);
    if (fs.is_open()) {
        while (getline(fs, line)) {
            if (to_upper(line).find("MESSAGE-ID:") != string::npos) {
                message_id = line.substr(line.find(":") + 2, line.length());
            }
        }    
        fs.close();
    }

    stringstream output;
    output << "223 " << current_article << " " << message_id << " retrieved";
    return output.str();
}

string get_article(deque<string> &args) {
    if (args.size() > 1) {
        return "501 Syntax error";
    }
    if ((args.size() == 0 && current_group.empty()) ||
        (args.size() == 1 && is_int(args.front()) && current_group.empty())) {
        return "412 No newsgroup selected";
    }
    if (args.size() == 0 && current_article == 0) {
        return "420 No current article selected";
    }
    stringstream output;
    int requested_article = -1;
    if (args.size() == 1) {
        string arg = args.front();
        try {
            if (arg.find("<") != string::npos && arg.find(">") != string::npos) {
                string message_id = arg.substr(arg.find("<"), arg.find(">") + 1);
                ifstream fs(get_article_path(message_id));
                if (fs.is_open()) { 
                    output << "220 0 " << message_id << "\n";
                    string line;
                    while (getline(fs, line)) {
                        output << line << "\n";
                    }    
                    fs.close();
                    output << ".";
                    return output.str();
                } else {
                    return "430 No such article found";
                }
            } else {
                requested_article = stoi(arg);
            } 
        } catch (...) {
            return "501 Syntax error";
        }
    }

    // requested article is in current group
    string path = "db/" + current_group;
    string filename = requested_article != -1 
        ? to_string(requested_article) + ".txt"
        : to_string(current_article) + ".txt";
    ifstream fs(path + "/" + filename);
    if (fs.is_open()) {
        current_article = requested_article != -1 ? requested_article : current_article;
        string line;
        string message_id;
        string article_text = "";

        while (getline(fs, line)) {
            article_text += line;
            article_text += "\n";
            if (to_upper(line).find("MESSAGE-ID:") != string::npos) {
                message_id = line.substr(line.find(":") + 2, line.length());
            }
        }    
        fs.close();
        output << "220 " << current_article << " " << message_id << "\n";
        output << article_text << ".";
        return output.str();
    } else {
        return "423 No article with that number";
    }
}

// returns current UTC date and time as specified in RFC
// i.e., yyyymmddhhmmss
string get_date(deque<string> &args) {
    if (!args.empty()) {
        return "501 Syntax error";
    }
    std::time_t now = std::time(0);
    std::tm* now_utc = std::gmtime(&now);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y%m%d%X", now_utc);
    string output(buffer);
    return "111 " + remove_char(output, ':');
}

string get_header(deque<string> &args) {
    if (args.size() != 2) {
        return "501 Syntax error";
    }
    string message_id = args.back();
    if (message_id.find("<") == string::npos || message_id.find(">") == string::npos) {
        return "501 Syntax error";
    }
    string field = to_upper(args.front());
    unordered_set<string> headers = check_all_headers();
    bool valid_header = false;
    for (auto h : headers) {
        if (field == to_upper(h)) {
            valid_header = true;
        }
    }
    if (!valid_header) {
        return "503 HDR not permitted on " + args.front();
    }
    string header = "";
    if (field.find(":") != string::npos) {
        if (field == ":BYTES") {
            header = count_file_bytes(get_article_path(message_id));
        }
        if (field == ":LINES") {
            header = count_file_body_lines(get_article_path(message_id));
        }
    } else {
        ifstream fs(get_article_path(message_id));
        if (fs.is_open()) {
            string line;
            while (getline(fs, line)) {
                if (to_upper(line).find(field) != string::npos) {
                    header = line.substr(to_upper(line).find(field) + field.length() + 2, line.length());
                    break;
                }
            }
            fs.close();
        } else {
            return "430 No such article found";
        }
    }
    stringstream output;
    output << "225 Header information follows\n";
    output << "0 " << header << "\n";
    output << ".";
    return output.str();
}

string get_list(deque<string> &args) {
    if (args.size() > 2) {
        return "501 Syntax error";
    }
    if (args.size() == 0) {
        return get_list_active(args);
    }
    if (to_upper(args.front()) == "ACTIVE") {
        args.pop_front();
        return get_list_active(args);
    }
    if (to_upper(args.front()) == "NEWSGROUPS") {
        args.pop_front();
        return get_list_newsgroups(args);
    }
    if (to_upper(args.front()) == "HEADERS") {
        args.pop_front();
        return get_list_headers(args);
    }
    return "501 Syntax error";
}

string get_list_active(deque<string> &args) {
    string wildmat = args.size() == 1 ? args.front() : "";

    vector<string> list;
    DIR *db = opendir("db");
    struct dirent *newsgroup;
    while ((newsgroup = readdir(db)) != NULL) {
        string dir_name = newsgroup->d_name;
        if (dir_name != "." && dir_name != "..") {
            if (!wildmat.empty() && !match(dir_name, wildmat)) {
                continue;
            }
            string list_item = dir_name + " ";
            string path = "db/" + dir_name;
            DIR *newsgroup_dir = opendir(path.c_str());
            struct dirent *article;
            int min = 0, max = 0;
            while ((article = readdir(newsgroup_dir)) != NULL) {
                string filename = article->d_name;
                if (filename.find(".txt") != string::npos) {
                    int watermark = stoi(filename.substr(0, filename.find(".")));
                    min = (watermark < min || min == 0) ? watermark : min;
                    max = (watermark > max) ? watermark : max;
                }
            }
            closedir(newsgroup_dir); 
            list_item += to_string(max) + " " + to_string(min) + " n";
            list.push_back(list_item);
        }
    }
    closedir(db);
    stringstream output;
    output << "215 information follows" << "\n";
    for (auto n : list) {
        output << n << "\n";
    }
    output << ".";

    return output.str();
}

string get_list_newsgroups(deque<string> &args) {
    string wildmat = args.size() == 1 ? args.front() : "";

    vector<string> list;
    DIR *db = opendir("db");
    struct dirent *newsgroup;
    while ((newsgroup = readdir(db)) != NULL) {
        string dir_name = newsgroup->d_name;
        if (dir_name != "." && dir_name != "..") {
            if (!wildmat.empty() && !match(dir_name, wildmat)) {
                continue;
            }
            string list_item = dir_name + " ";
            string info_path = "db/" + dir_name + "/.info";
            ifstream fs(info_path);
            if (fs.is_open()) {
                string line;
                while (getline(fs, line)) {
                    list_item += trim(line);
                    list_item += " ";
                }
                fs.close();
            }
            list.push_back(list_item);
        }
    }
    closedir(db);
    stringstream output;
    output << "215 information follows\n";
    for (auto n : list) {
        output << n << "\n";
    }
    output << ".";

    return output.str();
}

string get_list_headers(deque<string> &args) {
    string wildmat = args.size() == 1 ? args.front() : "";

    unordered_set<string> headers = check_all_headers();
    stringstream output;
    output << "215 headers and metadata items supported\n";
    for (auto h : headers) {
        if (!wildmat.empty() && !match(h, wildmat)) {
            continue;
        }
        output << h << "\n";
    }
    output << ".";
    return output.str();
}

string get_capabilities(deque<string> &args) {
    if (!args.empty()) {
        return "501 Syntax error";
    }
    stringstream output;
    output << "101 Capability list:\n" << "VERSION 2\n" << "READER\n" 
        << "LIST ACTIVE NEWSGROUPS HEADERS\n" << "HDR\n" << ".";
    return output.str();
}

string get_help(deque<string> &args) {
    if (!args.empty()) {
        return "501 Syntax error";
    }
    stringstream output;
    output << "100 Help text follows\n" 
        << "LIST ACTIVE [wildmat] - returns a list of all matching active newsgroups in the database with their reported high and low water marks\n"
        << "LIST NEWSGROUPS [wildmat] - returns a list of all matching newsgroups in the database with short descriptions\n"
        << "GROUP group - selects the specified group and returns the number of articles, low water mark, and high water mark for the group\n"
        << "LISTGROUP [group [range]] - same function as GROUP but lists all article numbers in the specified range (optional)\n"
        << "ARTICLE [message-id|number] - returns the currently selected article or the article specified by message-id or number\n"
        << "NEXT - selects the next available article in the currently selected group\n"
        << "HDR field message-id - returns the content of the specified header/field in the article specified by message-id\n"
        << "LIST HEADERS [wildmat] - returns a list of all headers and metadata items supported for querying with the HDR command\n"
        << "DATE - returns the current UTC date and time formatted as yyyymmddhhmmss\n"
        << "CAPABILITIES - returns information about what capabilities are supported by the server\n"
        << "HELP - returns a list of commands and their descriptions\n"
        << "QUIT - closes the connection to the server\n"
        << ".";
    return output.str();
}

string get_quit(deque<string> &args) {
    if (!args.empty()) {
        return "501 Syntax error";
    }
    return "205 Connection closing";
}

string count_file_bytes(string path) {
   ifstream fs(path, std::ios::binary);
   fs.seekg(0, std::ios::end);
   return to_string(fs.tellg());
}

string count_file_body_lines(string path) {
    int num_lines = 0;
    ifstream fs(path);
    if (fs.is_open()) {
        string line;
        bool is_body = false;
        while (getline(fs, line)) {
            if (line[0] == '\r' || line[0] == '\n' || line[0] == ' ') {
                is_body = true;
            } else if (is_body) {
                num_lines++;
            }
        }
        fs.close();
    }
    return to_string(num_lines - 1); 
}

string get_article_path(string message_id) {
    DIR *db = opendir("db");
    struct dirent *newsgroup;
    while ((newsgroup = readdir(db)) != NULL) {
        string newsgroup_name = newsgroup->d_name;
        if (newsgroup_name != "." && newsgroup_name != "..") {
            string path = "db/" + newsgroup_name;
            DIR *newsgroup_dir = opendir(path.c_str());
            struct dirent *article;
            while ((article = readdir(newsgroup_dir)) != NULL) {
                string article_name = article->d_name;
                if (article_name != "." && article_name != ".." && article_name != ".info") {
                    ifstream fs(path + "/" + article_name);
                    if (fs.is_open()) {
                        string line;
                        while (getline(fs, line)) {
                            if (to_upper(line).find("MESSAGE-ID:") != string::npos) {
                                string current_message_id = line.substr(line.find(":") + 2, line.length());
                                if (current_message_id.compare(message_id) == 0) {
                                    // found it, cleanup and return
                                    fs.close();
                                    closedir(newsgroup_dir);
                                    closedir(db);
                                    return path + "/" + article_name; 
                                }
                            }
                        }    
                        fs.close();
                    }
                }
            }
            closedir(newsgroup_dir);
        }
    }
    closedir(db);
    return "";
}

unordered_set<string> check_all_headers() {
    unordered_set<string> headers;
    // iterate through all articles
    DIR *db = opendir("db");
    struct dirent *newsgroup;
    while ((newsgroup = readdir(db)) != NULL) {
        string newsgroup_name = newsgroup->d_name;
        if (newsgroup_name != "." && newsgroup_name != "..") {
            string path = "db/" + newsgroup_name;
            DIR *newsgroup_dir = opendir(path.c_str());
            struct dirent *article;
            while ((article = readdir(newsgroup_dir)) != NULL) {
                string article_name = article->d_name;
                if (article_name != "." && article_name != ".." && article_name != ".info") {
                    ifstream fs(path + "/" + article_name);
                    if (fs.is_open()) {
                        string line;
                        while (getline(fs, line)) {
                            if (line[0] == '\r' || line[0] == '\n' || line[0] == ' ' || 
                                line.find(":") == string::npos) {
                                break;
                            }
                            headers.insert(line.substr(0, line.find(":")));
                        }    
                        fs.close();
                    }
                }
            }
            closedir(newsgroup_dir);
        }
    }
    closedir(db);
    headers.insert(":bytes");
    headers.insert(":lines");
    return headers;
}

string get_newsgroup_name(string requested_newsgroup) {
    DIR *db = opendir("db");
    struct dirent *newsgroup;
    while ((newsgroup = readdir(db)) != NULL) {
        string newsgroup_name = newsgroup->d_name;
        if (to_upper(newsgroup_name) == to_upper(requested_newsgroup)) {
            closedir(db);
            return newsgroup_name;
        }
    }
    closedir(db);
    return "";
}

void init_command_map() {
    command_map["GROUP"] = group;
    command_map["LISTGROUP"] = listgroup;
    command_map["NEXT"] = next;
    command_map["ARTICLE"] = article;
    command_map["HDR"] = hdr;
    command_map["LIST"] = list;
    command_map["DATE"] = date;
    command_map["CAPABILITIES"] = capabilities;
    command_map["HELP"] = help;
    command_map["QUIT"] = quit;
}
