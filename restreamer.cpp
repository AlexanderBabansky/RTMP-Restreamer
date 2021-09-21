#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <list>
#include <thread>
#include <future>
#include <limits.h>
#include "json.hpp"
#include "cxxopts.hpp"
#include "easyrtmp/data_layers/tcp_network.h"
#include "easyrtmp/rtmp_server_session.h"
#include "easyrtmp/rtmp_client_session.h"
#include "easyrtmp/utils.h"
#ifdef USE_OPENSSL
#include "easyrtmp/data_layers/openssl_tls.h"
#endif // USE_OPENSSL


using namespace std;
using json = nlohmann::json;

string config_path = "config.json";
uint16_t port = 1935;
string cert_path, key_path;
bool use_tls = false;
map<string, list<string>> route_matrix;

template<typename T>
void set_optional_parameter(cxxopts::ParseResult& parsed, T& out, const char* par_name) {
    try {
        out = parsed[par_name].as<T>();
    }
    catch (...) {}
}
struct ClientStruct {
    bool running_flag = true;
    std::thread client_thread;
    std::shared_ptr<TCPNetwork> tcp_network;
};
std::list<ClientStruct*> clients;

struct Destination{
    std::shared_ptr<TCPNetwork> tcp_network;
    std::shared_ptr<librtmp::RTMPEndpoint> endpoint;
    std::shared_ptr<librtmp::RTMPClientSession> session;
#ifdef USE_OPENSSL
    std::shared_ptr<OpenSSL_TLS> tls_network;
#endif // USE_OPENSSL
    DataLayer* transport_layer = nullptr;
    void SendRTMPMessage(librtmp::RTMPMediaMessage message) {
        session->SendRTMPMessage(message);
    }
};

void ClientVoid2(DataLayer* transport_level) {
    librtmp::RTMPEndpoint rtmp_endpoint(transport_level);
    librtmp::RTMPServerSession server_session(&rtmp_endpoint);
    librtmp::ClientParameters* client_parameters = nullptr;
    std::list<Destination> destinations;

    bool key_checked = false;
    bool first_run = true;

    while (true) {
        auto message = server_session.GetRTMPMessage();
        if (first_run) {
            client_parameters = server_session.GetClientParameters();            
            if (!client_parameters->key.size()) {
                cout << "Streaming without key, terminate" << endl;
                break;
            }
            if (route_matrix.find(client_parameters->key) == route_matrix.end()) {
                cout << "Key not found in config" << endl;
                break;
            }
            cout << "Restreaming for " << client_parameters->key << ":" << endl;
            first_run = false;

            for (auto& i : route_matrix[client_parameters->key]) {
                Destination dest;
                auto parsed = librtmp::ParseURL(i);
                client_parameters->app = parsed.app;
                client_parameters->url = parsed.url;
                client_parameters->key = parsed.key;
                cout << "   " << client_parameters->url << ", key:" << client_parameters->key << endl;
                TCPClient tcp_client;
                dest.tcp_network = tcp_client.ConnectToHost(parsed.url.c_str(),parsed.port);
                dest.transport_layer = dest.tcp_network.get();
                if (parsed.type == librtmp::ProtoType::RTMPS) {
#ifdef USE_OPENSSL
                    OpenSSL_TLS_Client tls_client;
                    dest.tls_network = tls_client.handshake(dest.transport_layer,client_parameters->url.c_str());
                    dest.transport_layer = dest.tls_network.get();
#else
                    cout << "Built wihout OpenSSL support. RTMPS not supported" << endl;
                    return;
#endif // USE_OPENSSL
                }
                dest.endpoint = std::make_shared<librtmp::RTMPEndpoint>(dest.transport_layer);
                dest.session = std::make_shared<librtmp::RTMPClientSession>(dest.endpoint.get());
                dest.session->SendClientParameters(client_parameters);
                destinations.push_back(dest);
            }
            cout << endl;
        }

        list<future<void>> async_tasks;
        for (auto& i : destinations) {
            async_tasks.push_back(std::async(std::launch::async, &Destination::SendRTMPMessage, &i, message));
        }
        if (message.message_type == librtmp::RTMPMessageType::VIDEO) {
            
        }
        for (auto& i : async_tasks) {
            i.wait();
        }
    }
}

void ClientVoid1(ClientStruct* cs) {
    try {
        DataLayer* transport_level = cs->tcp_network.get();
        if (use_tls) {
#ifdef USE_OPENSSL
            OpenSSL_TLS_Server tls_server;
            auto tls_layer = tls_server.handshake(transport_level, cert_path.c_str(), key_path.c_str());
            transport_level = tls_layer.get();
            ClientVoid2(transport_level);
#else
            cout << "Built without OpenSSL support" << endl;
#endif
        }
        else {
            ClientVoid2(transport_level);
        }
    }
    catch (...) {}
    cs->running_flag = false;
    cs->tcp_network->destroy();
}

void ServerThread(TCPServer* server) {
    try {
        while (true) {
            ClientStruct* cs = new ClientStruct();
            cs->tcp_network = server->accept();
            for (auto i = clients.begin(); i != clients.end();) {
                auto old_i = i;
                i++;
                if (!((*old_i)->running_flag)) {
                    (*old_i)->client_thread.join();
                    delete (*old_i);
                    clients.erase(old_i);
                }
            }
            cs->client_thread = thread(&ClientVoid1, cs);
            clients.push_back(cs);
        }
    }
    catch (...) {}
}

int main(int argc, char** argv) {
    std::setlocale(LC_ALL, "en_US.UTF-8");
    cxxopts::Options options("RTMP restreamer", "Portable RTMP server. Receives any RTMP stream and restream it to multiple destinations.");
    options.add_options()
        ("p,port", "network port [1935]", cxxopts::value<uint16_t>())
        ("c,config", "config file. Check out examples about format", cxxopts::value<std::string>())
        ("cert", "TLS public certificate filepath", cxxopts::value<std::string>())
        ("k,key", "TLS private key filepath", cxxopts::value<std::string>())
        ("h,help", "help");
    auto result = options.parse(argc, argv);
    if (result.count("help"))
    {
        cout << options.help() << endl;
        return 0;
    }
    cout << options.help() << endl << endl;

    set_optional_parameter<uint16_t>(result, port, "port");
    set_optional_parameter<string>(result, config_path, "config");
    set_optional_parameter<string>(result, cert_path, "cert");
    set_optional_parameter<string>(result, key_path, "key");

    if ((!cert_path.size() || !key_path.size()) && (cert_path.size() || cert_path.size())) {
        cout << "Error: set cert and key to enable TLS server" << endl;
        return 0;
    }
    if (cert_path.size())
        use_tls = true;
#ifndef USE_OPENSSL
    if (use_tls) {
        cout << "Tools was built without TLS support";
        return 0;
    }
#endif // !USE_OPENSSL

    ifstream config_file(config_path.c_str());
    if (!config_file.is_open()) {
        cout << "Config file not found" << endl;
        return -1;
    }
    json config_json;
    try {
        config_file >> config_json;
        config_file.close();
        for (auto i = config_json.begin(); i != config_json.end(); i++) {
            auto key = i.key();
            auto key_dests = *i;
            for (auto& k : key_dests) {
                route_matrix[i.key()].push_back(k.get<string>());
            }
        }
    }
    catch (json::exception& e) {
        cout << "Config file json format error" << endl;
        return -1;
    }
    cout << "Starting server..." << endl;
    try {
        TCPServer tcp_server(port);
        thread th(&ServerThread, &tcp_server);
        cout << "Started" << endl;
        this_thread::sleep_for(std::chrono::seconds(UINT_MAX));
    }
    catch (TCPNetworkException& net_exception) {
        cout << "Network error" << endl;
    }
    catch (exception&) {
        cout << "Unexpected error" << endl;
    }
	return 0;
}