#include "networking.h"
#include "logger.h"
#define ASIO_STANDALONE
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/config/debug_asio_no_tls.hpp>
#include "functionality.h"


typedef websocketpp::client<websocketpp::config::asio_client> Client;
typedef websocketpp::server<websocketpp::config::debug_asio> Server;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

struct WebsocketConnection {
    WebsocketConnection(const std::string& u, Client::connection_ptr c, bool connected) : uri(u), con(c), is_connected(connected) {}
    std::string uri;
    Client::connection_ptr con;
    bool is_connected;
};
static std::vector<std::thread> threads;
static std::mutex connection_mutex;
static std::vector<WebsocketConnection> connections;
static Server* server = nullptr;
static std::vector<websocketpp::connection_hdl> server_hdls;





static void on_message(Client* c, websocketpp::connection_hdl hdl, message_ptr msg) {
    // std::cout << "on_message called with hdl: " << hdl.lock().get() << " and message: " << msg->get_payload() << std::endl;
    try {
        nlohmann::json msg_json = nlohmann::json::parse(msg->get_payload());
        const auto& player_info = msg_json.find("info");
        if(player_info != msg_json.end()) {
            std::string data = GetLocalPlayerData();
            auto con = c->get_con_from_hdl(hdl);
            websocketpp::lib::error_code err = con->send(data);
            if(!err) {
                Log(err.message(), LOG_LEVEL::LOG_WARNING);
            }
        }
    }
    catch(...) {

    }
}
static void ConnectToWebsocket(const std::string& uri) {
    Client c;
    try {
        c.set_access_channels(websocketpp::log::alevel::none);
        c.clear_access_channels(websocketpp::log::alevel::frame_payload);

        c.init_asio();
        c.set_message_handler(bind(&on_message,&c,::_1,::_2));

        websocketpp::lib::error_code ec;
        Client::connection_ptr con = c.get_connection(uri, ec);
        if (ec) {
            std::cout << "could not create connection because: " << ec.message() << std::endl;
            return;
        }
        c.connect(con);

        connection_mutex.lock();
        connections.emplace_back(uri, con, false);


        connection_mutex.unlock();

        c.run();
    } catch (websocketpp::exception const & e) {
        std::cout << e.what() << std::endl;
    }
}

static bool validate(Server* s, websocketpp::connection_hdl conn) {
    connection_mutex.lock();
    server_hdls.push_back(conn);
    connection_mutex.unlock();

    return true;
}
static void on_fail(Server* s, websocketpp::connection_hdl hdl) {
    Server::connection_ptr con = s->get_con_from_hdl(hdl);
    //std::cout << "Fail handler: " << con->get_ec() << " " << con->get_ec().message()  << std::endl;
}

static void on_close(websocketpp::connection_hdl) {
}
static void on_http(Server* s, websocketpp::connection_hdl hdl) {
    Server::connection_ptr con = s->get_con_from_hdl(hdl);

    std::string res = con->get_request_body();

    std::stringstream ss;
    ss << "got HTTP request with " << res.size() << " bytes of body data.";

    con->set_body(ss.str());
    con->set_status(websocketpp::http::status_code::ok);
}
void on_message_server(Server* s, websocketpp::connection_hdl hdl, message_ptr msg) {
    try {
        nlohmann::json msg_json = nlohmann::json::parse(msg->get_payload());
        const auto& player_info = msg_json.find("info");
        if(player_info != msg_json.end()) {
            std::string data = GetLocalPlayerData();
            auto con = s->get_con_from_hdl(hdl);
            s->send(hdl, data, msg->get_opcode());
        }
    } catch (...) {
    }
}
void Net_StartListening(uint32_t port) {
   Server s;
   server = &s;
   Log(string_format("start_listening on: %d", port), LOG_LEVEL::LOG_INFO);
    try {
        // Set logging settings
        s.set_access_channels(websocketpp::log::alevel::none);
        s.clear_access_channels(websocketpp::log::alevel::frame_payload);

        // Initialize ASIO
        s.init_asio();
        s.set_reuse_addr(true);

        // Register our message handler
        s.set_message_handler(bind(&on_message_server,&s,::_1,::_2));

        s.set_http_handler(bind(&on_http,&s,::_1));
        s.set_fail_handler(bind(&on_fail,&s,::_1));
        s.set_close_handler(&on_close);

        s.set_validate_handler(bind(&validate,&s,::_1));

        s.listen(port);

        // Start the server accept loop
        s.start_accept();

        // Start the ASIO io_service run loop
        s.run();
    } catch (websocketpp::exception const & e) {
        std::cout << "err: " << e.what() << std::endl;
    } catch (const std::exception & e) {
        std::cout << "err: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "other exception" << std::endl;
    } 
}
void Net_ConnectToWebsocket(const std::string& uri) {
    ConnectToWebsocket(uri);
}

void Net_SendData(const std::string& json_str) {
    connection_mutex.lock();
    for(auto& con : connections) {
        websocketpp::lib::error_code err_val = con.con->send(json_str);
        if(!err_val) {
            Log(err_val.message(), LOG_LEVEL::LOG_WARNING);
        }
    }
    for(websocketpp::connection_hdl hdl : server_hdls) {
        if(!hdl.expired()) {
            auto conn_ptr = server->get_con_from_hdl(hdl);
            if(conn_ptr) {
                websocketpp::lib::error_code err_val = conn_ptr->send(json_str);
                if(!err_val) {
                    Log(err_val.message(), LOG_LEVEL::LOG_WARNING);
                }
            }
        }
    }
    connection_mutex.unlock();
}

