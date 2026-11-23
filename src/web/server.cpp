#include "bot/search/search_bot.h"
#include "game/game.h"

#include <arpa/inet.h>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <random>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unordered_map>
#include <unistd.h>
#include <vector>

namespace {
using namespace chess;

std::string square_name(u8 square) {
    std::string result;
    result += char('a' + (square & 7)); result += char('1' + (square >> 3));
    return result;
}

std::string move_name(Move move) {
    std::string result = square_name(move.from()) + square_name(move.to());
    if (move.is_promotion()) {
        static constexpr char symbols[] = {'p', 'n', 'b', 'r', 'q', 'k'};
        result += symbols[index(move.promotion())];
    }
    return result;
}

std::string url_decode(const std::string& value) {
    std::string out;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            unsigned int byte = 0; std::istringstream(value.substr(i + 1, 2)) >> std::hex >> byte;
            out += static_cast<char>(byte); i += 2;
        } else out += value[i] == '+' ? ' ' : value[i];
    }
    return out;
}

std::string query_value(const std::string& target, const std::string& key) {
    const std::string marker = key + '=';
    std::size_t position = target.find(marker);
    if (position == std::string::npos) return {};
    position += marker.size();
    const std::size_t end = target.find('&', position);
    return url_decode(target.substr(position, end == std::string::npos ? end : end - position));
}

class WebGame {
public:
    std::string state_json() {
        const GameResult result = game_.result(); MoveList legal;
        if (result == GameResult::Ongoing) game_.legal_moves(legal);
        std::ostringstream json; json << "{\"board\":[";
        for (u8 square = 0; square < 64; ++square) {
            if (square) json << ',';
            Piece piece; Color color;
            if (!game_.board().piece_at(square, piece, color)) json << "null";
            else {
                static constexpr char symbols[] = {'p', 'n', 'b', 'r', 'q', 'k'};
                char symbol = symbols[index(piece)];
                if (color == Color::White) symbol = char(symbol - 'a' + 'A');
                json << '\"' << symbol << '\"';
            }
        }
        json << "],\"turn\":\"" << (game_.side_to_move() == Color::White ? "white" : "black")
             << "\",\"check\":" << (game_.in_check() ? "true" : "false")
             << ",\"result\":\"" << result_name(result) << "\",\"botThinking\":false,\"legal\":[";
        bool first = true;
        for (Move move : legal) { if (!first) json << ','; first = false; json << '\"' << move_name(move) << '\"'; }
        json << "]}"; return json.str();
    }

    bool play(const std::string& notation, const std::string& required_color = {}) {
        if ((!required_color.empty()) && required_color != (game_.side_to_move() == Color::White ? "white" : "black")) return false;
        MoveList moves; game_.legal_moves(moves);
        for (Move move : moves) {
            const std::string name = move_name(move);
            if (name == notation || (move.is_promotion() && notation.size() == 4 && move.promotion() == Piece::Queen && name.substr(0, 4) == notation)) {
                game_.play(move); play_bot_if_needed(); return true;
            }
        }
        return false;
    }

    void reset(const std::string& bot, int time_ms) {
        game_ = Game{}; bot_white_ = bot == "white" || bot == "both";
        bot_black_ = bot == "black" || bot == "both"; bot_time_ms_ = time_ms > 0 ? time_ms : 500;
        play_bot_if_needed();
    }

    bool play_bot_step() {
        if (game_.result() != GameResult::Ongoing) return false;
        const bool bot_turn = game_.side_to_move() == Color::White ? bot_white_ : bot_black_;
        if (!bot_turn) return false;
        ai::SearchBot bot(game_.board(), game_.side_to_move());
        game_.play(bot.choose_move(bot_time_ms_));
        return true;
    }

private:
    Game game_; bool bot_white_ = false; bool bot_black_ = false; int bot_time_ms_ = 500;
    static const char* result_name(GameResult result) {
        switch (result) {
            case GameResult::Ongoing: return "ongoing"; case GameResult::WhiteWins: return "white_wins";
            case GameResult::BlackWins: return "black_wins"; case GameResult::Stalemate: return "stalemate";
            case GameResult::FiftyMoveDraw: return "fifty_move_draw"; case GameResult::RepetitionDraw: return "repetition_draw";
            case GameResult::InsufficientMaterialDraw: return "insufficient_material";
        } return "unknown";
    }
    void play_bot_if_needed() {
        if (game_.result() != GameResult::Ongoing) return;
        if (!(game_.side_to_move() == Color::White ? bot_white_ : bot_black_)) return;
        ai::SearchBot bot(game_.board(), game_.side_to_move()); game_.play(bot.choose_move(bot_time_ms_));
    }
};

struct Room {
    explicit Room(std::string pass) : password(std::move(pass)) {}
    std::string password; WebGame game; std::array<int, 2> sockets{{-1, -1}}; std::mutex mutex;
};

std::mutex rooms_mutex, local_mutex;
std::unordered_map<std::string, std::shared_ptr<Room>> rooms;
struct LocalGame { WebGame game; std::mutex mutex; };
std::unordered_map<std::string, std::shared_ptr<LocalGame>> local_games;

std::shared_ptr<LocalGame> local_game_for(const std::string& target) {
    std::string session = query_value(target, "session");
    if (session.empty() || session.size() > 80) session = "default";
    std::lock_guard<std::mutex> lock(local_mutex);
    auto& game = local_games[session];
    if (!game) game = std::make_shared<LocalGame>();
    return game;
}

std::string read_static(const std::string& file) {
    std::ifstream input(std::string(CHESS_WEB_ROOT) + '/' + file, std::ios::binary);
    if (!input) return {};
    std::ostringstream content; content << input.rdbuf(); return content.str();
}

void send_all(int fd, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) { const ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL); if (n <= 0) break; sent += std::size_t(n); }
}

void respond(int client, int status, const std::string& type, const std::string& body) {
    std::ostringstream out; out << "HTTP/1.1 " << status << (status == 200 ? " OK" : " Error")
        << "\r\nContent-Type: " << type << "\r\nContent-Length: " << body.size()
        << "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n" << body; send_all(client, out.str());
}

uint32_t rol(uint32_t value, int bits) { return (value << bits) | (value >> (32 - bits)); }
std::string websocket_accept(const std::string& key) {
    std::string data = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const uint64_t bit_length = uint64_t(data.size()) * 8; data += char(0x80);
    while ((data.size() % 64) != 56) data += char(0);
    for (int i = 7; i >= 0; --i) data += char((bit_length >> (i * 8)) & 0xff);
    uint32_t h[] = {0x67452301,0xefcdab89,0x98badcfe,0x10325476,0xc3d2e1f0};
    for (std::size_t chunk = 0; chunk < data.size(); chunk += 64) {
        uint32_t w[80]{};
        for (int i=0;i<16;++i) for(int j=0;j<4;++j) w[i]=(w[i]<<8)|uint8_t(data[chunk+i*4+j]);
        for(int i=16;i<80;++i) w[i]=rol(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4];
        for(int i=0;i<80;++i){ uint32_t f,k;if(i<20){f=(b&c)|(~b&d);k=0x5a827999;}else if(i<40){f=b^c^d;k=0x6ed9eba1;}else if(i<60){f=(b&c)|(b&d)|(c&d);k=0x8f1bbcdc;}else{f=b^c^d;k=0xca62c1d6;}uint32_t t=rol(a,5)+f+e+k+w[i];e=d;d=c;c=rol(b,30);b=a;a=t;}
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;
    }
    std::string raw; for(uint32_t v:h) for(int i=3;i>=0;--i) raw+=char((v>>(i*8))&0xff);
    static constexpr char b64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out; for(std::size_t i=0;i<raw.size();i+=3){uint32_t n=uint8_t(raw[i])<<16;if(i+1<raw.size())n|=uint8_t(raw[i+1])<<8;if(i+2<raw.size())n|=uint8_t(raw[i+2]);out+=b64[(n>>18)&63];out+=b64[(n>>12)&63];out+=i+1<raw.size()?b64[(n>>6)&63]:'=';out+=i+2<raw.size()?b64[n&63]:'=';} return out;
}

bool recv_exact(int fd, void* output, std::size_t size) {
    auto* bytes = static_cast<unsigned char*>(output); std::size_t got=0;
    while(got<size){ssize_t n=::recv(fd,bytes+got,size-got,0);if(n<=0)return false;got+=std::size_t(n);}return true;
}

bool receive_frame(int fd, std::string& payload) {
    unsigned char h[2]; if(!recv_exact(fd,h,2)) return false;
    const int opcode=h[0]&15; uint64_t size=h[1]&127;
    if(opcode==8) return false;
    if(size==126){unsigned char x[2];if(!recv_exact(fd,x,2))return false;size=(uint64_t(x[0])<<8)|x[1];}
    else if(size==127){unsigned char x[8];if(!recv_exact(fd,x,8))return false;size=0;for(unsigned char c:x)size=(size<<8)|c;}
    if(size>4096) return false;
    unsigned char mask[4]{}; if((h[1]&128)&&!recv_exact(fd,mask,4))return false;
    payload.resize(size); if(size&&!recv_exact(fd,payload.data(),size))return false;
    if(h[1]&128) for(std::size_t i=0;i<size;++i)payload[i]^=mask[i%4];
    return opcode==1;
}

void send_frame(int fd, const std::string& text) {
    std::string frame; frame += char(0x81);
    if(text.size()<126) frame+=char(text.size()); else {frame+=char(126);frame+=char((text.size()>>8)&255);frame+=char(text.size()&255);}
    frame += text; send_all(fd,frame);
}

std::string room_state(Room& room, int slot, const std::string& code) {
    const int players=(room.sockets[0]>=0)+(room.sockets[1]>=0);
    return "{\"type\":\"state\",\"player\":\""+std::string(slot==0?"white":"black")+"\",\"room\":\""+code+"\",\"players\":"+std::to_string(players)+",\"game\":"+room.game.state_json()+"}";
}

void broadcast(Room& room, const std::string& code) {
    for(int i=0;i<2;++i) if(room.sockets[i]>=0) send_frame(room.sockets[i],room_state(room,i,code));
}

std::string header_value(const std::string& request, const std::string& name) {
    std::string wanted;
    wanted.reserve(name.size());
    for (unsigned char character : name) wanted += static_cast<char>(std::tolower(character));

    std::size_t line_start = request.find("\r\n");
    if (line_start == std::string::npos) return {};
    line_start += 2;
    while (line_start < request.size()) {
        const std::size_t line_end = request.find("\r\n", line_start);
        if (line_end == std::string::npos || line_end == line_start) break;
        const std::size_t colon = request.find(':', line_start);
        if (colon != std::string::npos && colon < line_end) {
            std::string current;
            current.reserve(colon - line_start);
            for (std::size_t i = line_start; i < colon; ++i)
                current += static_cast<char>(std::tolower(static_cast<unsigned char>(request[i])));
            if (current == wanted) {
                std::size_t value_start = colon + 1;
                while (value_start < line_end && std::isspace(static_cast<unsigned char>(request[value_start]))) ++value_start;
                std::size_t value_end = line_end;
                while (value_end > value_start && std::isspace(static_cast<unsigned char>(request[value_end - 1]))) --value_end;
                return request.substr(value_start, value_end - value_start);
            }
        }
        line_start = line_end + 2;
    }
    return {};
}

void websocket_session(int client, const std::string& request, const std::string& target) {
    const std::string code=query_value(target,"room"), password=query_value(target,"password");
    std::shared_ptr<Room> room; {std::lock_guard<std::mutex> lock(rooms_mutex);auto it=rooms.find(code);if(it!=rooms.end())room=it->second;}
    if(!room){respond(client,404,"text/plain","Room not found");return;}
    int slot=-1;
    {
        std::lock_guard<std::mutex> lock(room->mutex);
        if(room->password!=password){respond(client,403,"text/plain","Wrong password");return;}
        if(room->sockets[0]<0)slot=0;else if(room->sockets[1]<0)slot=1;else{respond(client,409,"text/plain","Room full");return;}
        const std::string key=header_value(request,"Sec-WebSocket-Key"); if(key.empty()){respond(client,400,"text/plain","Bad websocket");return;}
        std::ostringstream handshake;handshake<<"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: "<<websocket_accept(key)<<"\r\n\r\n";send_all(client,handshake.str());
        room->sockets[slot]=client; broadcast(*room,code);
    }
    std::string message;
    while(receive_frame(client,message)) {
        if(message.rfind("move:",0)==0){std::lock_guard<std::mutex> lock(room->mutex);if(room->game.play(message.substr(5),slot==0?"white":"black"))broadcast(*room,code);else send_frame(client,"{\"type\":\"error\",\"message\":\"Niedozwolony ruch\"}");}
    }
    {std::lock_guard<std::mutex> lock(room->mutex);if(room->sockets[slot]==client)room->sockets[slot]=-1;broadcast(*room,code);} ::close(client);
}

std::string new_room_code() {
    static std::mt19937 rng(std::random_device{}()); static std::mutex rng_mutex; static constexpr char chars[]="ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    std::lock_guard<std::mutex> lock(rng_mutex); std::string code(6,'A'); for(char& c:code)c=chars[rng()%(sizeof(chars)-1)]; return code;
}

void handle_client(int client) {
    char buffer[16384];const ssize_t size=::recv(client,buffer,sizeof(buffer)-1,0);if(size<=0){::close(client);return;}buffer[size]=0;
    const std::string raw(buffer,size);std::istringstream request(raw);std::string method,target,version;request>>method>>target>>version;
    if(target.rfind("/ws?",0)==0){websocket_session(client,raw,target);return;}
    if(target.rfind("/api/state",0)==0){auto game=local_game_for(target);std::lock_guard<std::mutex> lock(game->mutex);respond(client,200,"application/json",game->game.state_json());}
    else if(target.rfind("/api/move?",0)==0&&method=="POST"){auto game=local_game_for(target);std::lock_guard<std::mutex> lock(game->mutex);bool ok=game->game.play(query_value(target,"move"));respond(client,ok?200:400,"application/json",ok?game->game.state_json():"{\"error\":\"illegal_move\"}");}
    else if(target.rfind("/api/new",0)==0&&method=="POST"){auto game=local_game_for(target);std::lock_guard<std::mutex> lock(game->mutex);const auto bot=query_value(target,"bot"),time=query_value(target,"time");game->game.reset(bot.empty()?"black":bot,time.empty()?500:std::atoi(time.c_str()));respond(client,200,"application/json",game->game.state_json());}
    else if(target.rfind("/api/bot-step",0)==0&&method=="POST"){
        auto game=local_game_for(target);
        std::lock_guard<std::mutex> lock(game->mutex);
        const bool moved=game->game.play_bot_step();
        respond(client,moved?200:409,"application/json",moved?game->game.state_json():"{\"error\":\"bot_cannot_move\"}");
    }
    else if(target=="/api/rooms"&&method=="GET"){
        std::ostringstream json; json << '['; bool first=true;
        std::lock_guard<std::mutex> rooms_lock(rooms_mutex);
        for(const auto& entry:rooms){
            std::lock_guard<std::mutex> room_lock(entry.second->mutex);
            const int players=(entry.second->sockets[0]>=0)+(entry.second->sockets[1]>=0);
            if(players>=2)continue;
            if(!first)json<<',';
            first=false;
            json<<"{\"code\":\""<<entry.first<<"\",\"players\":"<<players<<'}';
        }
        json<<']';respond(client,200,"application/json",json.str());
    }
    else if(target.rfind("/api/room/create?",0)==0&&method=="POST"){
        const std::string password=query_value(target,"password");std::string code;
        {std::lock_guard<std::mutex> lock(rooms_mutex);do{code=new_room_code();}while(rooms.count(code));rooms.emplace(code,std::make_shared<Room>(password));}
        respond(client,200,"application/json","{\"room\":\""+code+"\"}");
    } else {
        std::string file="index.html",type="text/html; charset=utf-8";if(target=="/app.js"){file="app.js";type="application/javascript";}else if(target=="/style.css"){file="style.css";type="text/css";}else if(target!="/"){respond(client,404,"text/plain","Not found");::close(client);return;}const auto body=read_static(file);respond(client,body.empty()?404:200,type,body);
    }
    ::close(client);
}
} // namespace

int main(int argc,char** argv){
    const int port=argc>1?std::atoi(argv[1]):8080;const int server=::socket(AF_INET,SOCK_STREAM,0);if(server<0){std::perror("socket");return 1;}int reuse=1;setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_ANY);address.sin_port=htons(static_cast<uint16_t>(port));if(::bind(server,reinterpret_cast<sockaddr*>(&address),sizeof(address))<0||::listen(server,64)<0){std::perror("server");::close(server);return 1;}std::signal(SIGPIPE,SIG_IGN);std::cout<<"Chess web server: http://127.0.0.1:"<<port<<'\n';for(;;){const int client=::accept(server,nullptr,nullptr);if(client>=0)std::thread(handle_client,client).detach();}
}
