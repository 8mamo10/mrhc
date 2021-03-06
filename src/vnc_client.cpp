#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "opencv2/opencv.hpp"

#include <X11/Xlib.h>

#include "d3des.h"
#include "mrhc_common.h"
#include "vnc_client.h"

const std::string vnc_client::KEY_BACKSPACE = "Backspace";
const std::string vnc_client::KEY_PERIOD    = ".";
const std::string vnc_client::KEY_ENTER     = "Enter";
const std::string vnc_client::KEY_SPACE     = "Space";
const std::string vnc_client::KEY_SLASH     = "/";

// Since connect() can wait too long, add timeout to connect()
// This is effective when the input connection destination is wrong.
static int connect_with_timeout(int sockfd, struct sockaddr *addr, size_t addrlen, struct timeval *timeout)
{
    int res, opt;
    // get socket flags
    if ((opt = fcntl(sockfd, F_GETFL, NULL)) < 0) {
        return -1;
    }
    // set socket flags
    if (fcntl(sockfd, F_SETFL, opt | O_NONBLOCK) < 0) {
        return -1;
    }
    if ((res = connect(sockfd, addr, addrlen)) < 0) {
        if (errno == EINPROGRESS) {
            fd_set wait_set;
            // make file descriptor set with socket
            FD_ZERO (&wait_set);
            FD_SET(sockfd, &wait_set);
            // wait for socket to be writable; return after given timeout
            res = select(sockfd + 1, NULL, &wait_set, NULL, timeout);
        }
    } else {
        // connection was successful immediately
        res = 1;
    }
    // reset socket flags
    if (fcntl(sockfd, F_SETFL, opt) < 0) {
        return -1;
    }
    if (res < 0) {
        // an error occured in connect or select
        return -1;
    } else if (res == 0) {
        // select timed out
        errno = ETIMEDOUT;
        return -1;
    } else {
        // almost finished
        socklen_t len = sizeof(opt);
        // check for errors in socket layer
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &opt, &len) < 0) {
            return -1;
        }
        // there was an error
        if (opt) {
            errno = opt;
            return -1;
        }
    }
    return 0;
}

//// public /////

vnc_client::vnc_client(std::string host, int port, std::string password)
    : sockfd(0), host(host), port(port), password(password), version(""),  width(0), height(0), pixel_format({}), name("")
{
    memset(this->challenge, 0, sizeof(this->challenge));
}

vnc_client::~vnc_client()
{
    close(this->sockfd);
}

bool vnc_client::connect_to_server()
{
    if ((this->sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return false;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(this->port);
    addr.sin_addr.s_addr = inet_addr(this->host.c_str());

    LOGGER_DEBUG("host:%s", this->host.c_str());
    LOGGER_DEBUG("port:%d", this->port);

    //if (connect(this->sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0) {
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    if (connect_with_timeout(this->sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in), &timeout) < 0) {
        return false;
    }
    return true;
}

bool vnc_client::recv_protocol_version()
{
    protocol_version_t protocol_version = {};

    char buf[BUF_SIZE] = {};
    int recv_length = recv(this->sockfd, buf, sizeof(protocol_version), 0);
    if (recv_length < 0) {
        return false;
    }
    LOGGER_DEBUG("recv:%d", recv_length);
    LOGGER_DEBUG(buf);

    memmove(&protocol_version, buf, recv_length);

    if (memcmp(protocol_version.values, RFB_PROTOCOL_VERSION_3_3, sizeof(RFB_PROTOCOL_VERSION_3_3)) == 0) {
        this->version = std::string(buf);
        LOGGER_DEBUG("RFB Version 3.3");
    } else if (memcmp(protocol_version.values, RFB_PROTOCOL_VERSION_3_7, sizeof(RFB_PROTOCOL_VERSION_3_7)) == 0) {
        this->version = std::string(buf);
        LOGGER_DEBUG("RFB Version 3.7");
    } else if (memcmp(protocol_version.values, RFB_PROTOCOL_VERSION_3_8, sizeof(RFB_PROTOCOL_VERSION_3_8)) == 0) {
        this->version = std::string(buf);
        LOGGER_DEBUG("RFB Version 3.8");
    } else {
        LOGGER_DEBUG("Invalid RFB Version");
        return false;
    }
    return true;
}

bool vnc_client::send_protocol_version()
{
    protocol_version_t protocol_version = {};
    memmove(protocol_version.values, this->version.c_str(), this->version.length());
    // send back the same version string
    int send_length = send(this->sockfd, &protocol_version, sizeof(protocol_version), 0);
    if (send_length < 0) {
        return false;
    }
    LOGGER_DEBUG("send:%d", send_length);
    LOGGER_DEBUG((char*)&protocol_version);
    return true;
}

bool vnc_client::recv_supported_security_types()
{
    supported_security_types_t supported_security_types = {};

    char buf[BUF_SIZE] = {};
    int recv_length = recv(this->sockfd, buf, sizeof(supported_security_types), 0);
    if (recv_length < 0) {
        return false;
    }
    LOGGER_DEBUG("recv:%d", recv_length);
    LOGGER_XDEBUG(buf, recv_length);

    memmove(&supported_security_types, buf, recv_length);

    uint8_t num = supported_security_types.number_of_security_types;
    for (int i = 0; i < num; i++) {
        this->security_types.push_back(supported_security_types.security_types[i]);
    }
    return true;
}

bool vnc_client::send_security_type()
{
    security_type_t security_type = {};
    // specify VNC Authentication
    security_type.value = RFB_SECURITY_TYPE_VNC_AUTH;
    int send_length = send(this->sockfd, &security_type, sizeof(security_type), 0);
    if (send_length < 0) {
        return false;
    }
    LOGGER_DEBUG("send:%d", send_length);
    LOGGER_XDEBUG(((char*)&security_type), send_length);
    return true;
}

bool vnc_client::recv_vnc_auth_challenge()
{
    vnc_auth_challenge_t vnc_auth_challenge = {};

    char buf[BUF_SIZE] = {};
    int recv_length = recv(this->sockfd, buf, sizeof(vnc_auth_challenge), 0);
    if (recv_length < 0) {
        return false;
    }
    LOGGER_DEBUG("recv:%d", recv_length);
    LOGGER_XDEBUG(buf, recv_length);

    memmove(&vnc_auth_challenge, buf, recv_length);
    memmove(this->challenge, &vnc_auth_challenge, recv_length);
    return true;
}

bool vnc_client::send_vnc_auth_response()
{
    // DES
    deskey((unsigned char*)this->password.c_str(), EN0);
    for (int j = 0; j < RFB_VNC_AUTH_CHALLENGE_LENGTH; j += 8) {
        des(this->challenge+j, this->challenge+j);
    }
    int send_length = send(this->sockfd, this->challenge, RFB_VNC_AUTH_CHALLENGE_LENGTH, 0);
    if (send_length < 0) {
        return false;
    }
    LOGGER_DEBUG("send:%d", send_length);
    LOGGER_XDEBUG(this->challenge, send_length);
    return true;
}

bool vnc_client::recv_security_result()
{
    security_result_t security_result = {};

    char buf[BUF_SIZE] = {};
    int recv_length = recv(this->sockfd, buf, sizeof(security_result), 0);
    if (recv_length < 0) {
        return false;
    }
    LOGGER_DEBUG("recv:%d", recv_length);
    LOGGER_XDEBUG(buf, recv_length);

    memmove(&security_result, buf, recv_length);

    uint32_t status = ntohl(security_result.status);
    LOGGER_DEBUG("status:%lu", status);
    if (status != RFB_SECURITY_RESULT_OK) {
        LOGGER_DEBUG("VNC Authentication failed");
        return false;
    }
    LOGGER_DEBUG("VNC Authentication ok");
    return true;
}

bool vnc_client::send_client_init()
{
    client_init_t client_init = {};
    client_init.shared_flag = RFB_SHARED_FLAG_ON;

    int send_length = send(this->sockfd, &client_init, sizeof(client_init), 0);
    if (send_length < 0) {
        return false;
    }
    LOGGER_DEBUG("send:%d", send_length);
    LOGGER_XDEBUG(((char*)&client_init), send_length);
    return true;
}

bool vnc_client::recv_server_init()
{
    server_init_t server_init = {};

    char buf[BUF_SIZE] = {};
    int recv_length = recv(this->sockfd, buf, sizeof(server_init), 0);
    if (recv_length < 0) {
        return false;
    }
    LOGGER_DEBUG("recv:%d", recv_length);
    LOGGER_XDEBUG(buf, recv_length);

    memmove(&server_init, buf, recv_length);

    this->width = ntohs(server_init.frame_buffer_width);
    this->height = ntohs(server_init.frame_buffer_height);
    char name[BUF_SIZE] = {};
    memmove(name, server_init.name_string, ntohl(server_init.name_length));
    this->name = std::string(name);

    LOGGER_DEBUG("frame_buffer_width:%d", this->width);
    LOGGER_DEBUG("frame_buffer_height:%d", this->height);
    LOGGER_DEBUG("name:%s", this->name.c_str());

    return true;
}

bool vnc_client::send_set_pixel_format()
{
    set_pixel_format_t set_pixel_format = {};
    pixel_format_t pixel_format = {};
    pixel_format.bits_per_pixel = 0x20;
    pixel_format.depth = 0x20;
    pixel_format.big_endian_flag = 0x00;
    pixel_format.true_colour_flag = 0x01;
    pixel_format.red_max = htons(0xff);
    pixel_format.green_max = htons(0xff);
    pixel_format.blue_max = htons(0xff);
    pixel_format.red_shift = 0x10;
    pixel_format.green_shift = 0x08;
    pixel_format.blue_shift = 0x00;
    set_pixel_format.pixel_format = pixel_format;
    this->pixel_format = pixel_format;

    int send_length = send(this->sockfd, &set_pixel_format, sizeof(set_pixel_format), 0);
    if (send_length < 0) {
        return false;
    }
    LOGGER_DEBUG("send:%d", send_length);
    LOGGER_XDEBUG(((char*)&set_pixel_format), send_length);
    return true;
}

bool vnc_client::send_set_encodings()
{
    set_encodings_t set_encodings = {};
    set_encodings.number_of_encodings = htons(1);
    set_encodings.encoding_type = htonl(RFB_ENCODING_RAW);

    int send_length = send(this->sockfd, &set_encodings, sizeof(set_encodings), 0);
    if (send_length < 0) {
        return false;
    }
    LOGGER_DEBUG("send:%d", send_length);
    LOGGER_XDEBUG(((char*)&set_encodings), send_length);
    return true;
}

bool vnc_client::send_frame_buffer_update_request()
{
    frame_buffer_update_request_t frame_buffer_update_request = {};
    frame_buffer_update_request.incremental = RFB_INCREMENTAL_OFF;
    frame_buffer_update_request.x_position = htons(0);
    frame_buffer_update_request.y_position = htons(0);
    frame_buffer_update_request.width = htons(this->width);
    frame_buffer_update_request.height = htons(this->height);

    int send_length = send(this->sockfd, &frame_buffer_update_request, sizeof(frame_buffer_update_request), 0);
    if (send_length < 0) {
        return false;
    }
    LOGGER_DEBUG("send:%d", send_length);
    LOGGER_XDEBUG(((char*)&frame_buffer_update_request), send_length);
    return true;
}

bool vnc_client::recv_frame_buffer_update()
{
    LOGGER_DEBUG("recv frame_buffer_update");

    frame_buffer_update_t frame_buffer_update = {};

    char buf[BUF_SIZE] = {};
    // size - 1 because message type has already recv
    int recv_length = recv(this->sockfd, buf, sizeof(frame_buffer_update) - 1, 0);
    if (recv_length < 0) {
        return false;
    }
    LOGGER_DEBUG("recv:%d", recv_length);
    LOGGER_XDEBUG(buf, recv_length);

    memmove(&frame_buffer_update.padding, buf, recv_length);

    uint16_t number_of_rectangles = ntohs(frame_buffer_update.number_of_rectangles);
    LOGGER_DEBUG("number_of_rectangles:%d", number_of_rectangles);

    if (!this->recv_rectangles(number_of_rectangles)) {
        LOGGER_DEBUG("failed to recv_rectangles");
        return false;
    }
    return true;
}

bool vnc_client::recv_set_colour_map_entries()
{
    LOGGER_DEBUG("recv set_colour_map_entries");

    set_colour_map_entries_t set_colour_map_entries = {};

    char buf[BUF_SIZE] = {};
    // size - 1 because message type has already recv
    int recv_length = recv(this->sockfd, buf, sizeof(set_colour_map_entries) - 1, 0);
    if (recv_length < 0) {
        return false;
    }
    LOGGER_DEBUG("recv:%d", recv_length);
    LOGGER_XDEBUG(buf, recv_length);

    memmove(&set_colour_map_entries.padding, buf, recv_length);

    uint16_t number_of_colours = ntohs(set_colour_map_entries.number_of_colours);
    LOGGER_DEBUG("number_of_colours:%d", number_of_colours);

    if (!this->recv_colours(number_of_colours)) {
        LOGGER_DEBUG("failed to recv_colours");
        return false;
    }
    LOGGER_DEBUG("discarded");
    return true;
}

bool vnc_client::recv_bell()
{
    LOGGER_DEBUG("recv bell");
    LOGGER_DEBUG("discarded");
    return true;
}

bool vnc_client::recv_server_cut_text()
{
    LOGGER_DEBUG("recv server_cut_text");

    server_cut_text_t server_cut_text = {};

    char buf[BUF_SIZE] = {};
    // size - 1 because message type has already recv
    int recv_length = recv(this->sockfd, buf, sizeof(server_cut_text) - 1, 0);
    if (recv_length < 0) {
        return false;
    }
    LOGGER_DEBUG("recv:%d", recv_length);
    LOGGER_XDEBUG(buf, recv_length);

    memmove(&server_cut_text.padding, buf, recv_length);

    uint32_t length = ntohl(server_cut_text.length);
    LOGGER_DEBUG("length:%d", length);

    if (!this->recv_text(length)) {
        LOGGER_DEBUG("failed to recv_text");
        return false;
    }
    LOGGER_DEBUG("discarded");
    return true;
}

bool vnc_client::send_key_event(std::string key)
{
    key_event_t key_event = {};
    key_event.down_flag = RFB_KEY_DOWN;
    LOGGER_DEBUG("key:%s", key.c_str());
    uint32_t key_code = this->convert_key_to_code(key);
    if (key_code == 0) {
        LOGGER_DEBUG("Ignore unreconized key:%s", key.c_str());
        return true;
    }
    key_event.key = htonl(key_code);

    // send down
    int send_length = send(this->sockfd, &key_event, sizeof(key_event), 0);
    if (send_length < 0) {
        return false;
    }
    LOGGER_DEBUG("send:%d", send_length);
    LOGGER_XDEBUG(((char*)&key_event), send_length);

    // send up
    key_event.down_flag = RFB_KEY_UP;
    send_length = send(this->sockfd, &key_event, sizeof(key_event), 0);
    if (send_length < 0) {
        return false;
    }
    LOGGER_DEBUG("send:%d", send_length);
    LOGGER_XDEBUG(((char*)&key_event), send_length);
    return true;
}

bool vnc_client::send_pointer_event(uint16_t x_position, uint16_t y_position, uint8_t button)
{
    uint8_t button_mask = 0;
    button_mask |= (RFB_POINTER_DOWN << button);

    pointer_event_t pointer_event = {};
    pointer_event.button_mask = button_mask;
    pointer_event.x_position = htons(x_position);
    pointer_event.y_position = htons(y_position);

    // send down
    int send_length = send(this->sockfd, &pointer_event, sizeof(pointer_event), 0);
    if (send_length < 0) {
        return false;
    }
    LOGGER_DEBUG("send:%d", send_length);
    LOGGER_XDEBUG(((char*)&pointer_event), send_length);

    // send up
    button_mask = 0;
    pointer_event.button_mask = button_mask;
    send_length = send(this->sockfd, &pointer_event, sizeof(pointer_event), 0);
    if (send_length < 0) {
        return false;
    }
    LOGGER_DEBUG("send:%d", send_length);
    LOGGER_XDEBUG(((char*)&pointer_event), send_length);
    return true;
}

bool vnc_client::initialize()
{
    if (!this->connect_to_server()) {
        std::cerr << "Error: " << strerror(errno);
        LOGGER_DEBUG("Failed to connect_to_server");
        return false;
    }
    return true;
}

bool vnc_client::authenticate()
{
    // protocol version
    if (!this->recv_protocol_version()) {
        LOGGER_DEBUG("Failed to recv_protocol_version");
        return false;
    }
    if (!this->send_protocol_version()) {
        LOGGER_DEBUG("Failed to send_protocol_version");
        return false;
    }
    LOGGER_DEBUG("Exchanged protocol version");
    // security type
    if (!this->recv_supported_security_types()) {
        LOGGER_DEBUG("Failed to recv_supported_security_types");
        return false;
    }
    if (!this->send_security_type()) {
        LOGGER_DEBUG("Failed to send_security_type");
        return false;
    }
    LOGGER_DEBUG("Exchanged security type");
    // vnc auth
    if (!this->recv_vnc_auth_challenge()) {
        LOGGER_DEBUG("Failed to recv_vnc_auth_challenge");
        return false;
    }
    if (!this->send_vnc_auth_response()) {
        LOGGER_DEBUG("Failed to send_vnc_auth_response");
        return false;
    }
    if (!this->recv_security_result()) {
        LOGGER_DEBUG("Failed to recv_security_result");
        return false;
    }
    LOGGER_DEBUG("Authenticated");
    // client/server init
    if (!this->send_client_init()) {
        LOGGER_DEBUG("Failed to send_client_init");
        return false;
    }
    if (!this->recv_server_init()) {
        LOGGER_DEBUG("Failed to recv_server_init");
        return false;
    }
    LOGGER_DEBUG("Exchanged Client/Server Init");
    return true;
}

bool vnc_client::configure()
{
    // format/encode
    if (!this->send_set_pixel_format()) {
        LOGGER_DEBUG("Failed to send_set_pixel_format");
        return false;
    }
    if (!this->send_set_encodings()) {
        LOGGER_DEBUG("Failed to send_set_encodings");
        return false;
    }
    return true;
}

bool vnc_client::operate(vnc_operation_t operation)
{
    uint16_t x = operation.x;
    uint16_t y = operation.y;
    uint8_t button = operation.button;
    std::string key = operation.key;
    if (!key.empty()) {
        if (!this->send_key_event(key)) {
            LOGGER_DEBUG("Failed to send_key_event");
            return false;
        }
        return true;
    }
    if (x == 0 && y == 0) {
        // no pointer
        return true;
    }
    if (!this->send_pointer_event(x, y, button)) {
        LOGGER_DEBUG("Failed to send_pointer_event");
        return false;
    }
    return true;
}

bool vnc_client::capture(vnc_operation_t operation)
{
    std::string key = operation.key;
    if (!key.empty()) return true;

    this->clear_buf();

    if (!this->send_frame_buffer_update_request()) {
        LOGGER_DEBUG("Failed to send_frame_buffer_update_request");
        return false;
    }
    if (!this->recv_server_to_client_message()) {
        LOGGER_DEBUG("Failed to recv_server_to_client_message");
        return false;
    }
    // output image
    if (!this->draw_image()) {
        LOGGER_DEBUG("Failed to draw_image");
        return false;
    }
    // pointer image
    uint16_t x = operation.x;
    uint16_t y = operation.y;
    if (x == 0 && y == 0) {
        // no pointer
        return true;
    }
    if (!this->draw_pointer(x, y)) {
        LOGGER_DEBUG("Failed to draw_pointer");
        return false;
    }
    return true;
}

bool vnc_client::write_jpeg_buf(const std::string path)
{
    return cv::imwrite(path, this->image);
}

//// private /////

bool vnc_client::recv_server_to_client_message()
{
    char buf[BUF_SIZE] = {};
    // recv message type
    int recv_length = recv(this->sockfd, buf, 1, 0);
    if (recv_length < 0) {
        return false;
    }
    LOGGER_DEBUG("recv:%d", recv_length);
    LOGGER_XDEBUG(buf, recv_length);

    uint8_t message_type = 0;
    memmove(&message_type, buf, recv_length);
    switch (message_type) {
    case RFB_MESSAGE_TYPE_FRAME_BUFFER_UPDATE:
        return this->recv_frame_buffer_update();
    case RFB_MESSAGE_TYPE_SET_COLOUR_MAP_ENTRIES:
        return this->recv_set_colour_map_entries();
    case RFB_MESSAGE_TYPE_BELL:
        return this->recv_bell();
    case RFB_MESSAGE_TYPE_SERVER_CUT_TEXT:
        return this->recv_server_cut_text();
    default:
        LOGGER_DEBUG("unexpected message_type:%d", message_type);
        return false;
    }
    return true;
}

bool vnc_client::recv_rectangles(uint16_t number_of_rectangles)
{
    for (int i = 0; i < number_of_rectangles; i++) {
        LOGGER_DEBUG("--start recv_rectangle:%d", i + 1);
        if (!this->recv_rectangle()) {
            LOGGER_DEBUG("failed to recv_rectangle");
            return false;
        }
        LOGGER_DEBUG("--finish recv_rectangle:%d", i + 1);
    }
    return true;
}

bool vnc_client::recv_rectangle()
{
    pixel_data_t pixel_data = {};

    char buf[BUF_SIZE] = {};
    int recv_length = recv(this->sockfd, buf, sizeof(pixel_data), 0);
    if (recv_length < 0) {
        return false;
    }
    LOGGER_DEBUG("recv:%d", recv_length);
    LOGGER_XDEBUG(buf, recv_length);

    memmove(&pixel_data, buf, recv_length);
    uint16_t x_position = ntohs(pixel_data.x_position);
    uint16_t y_position = ntohs(pixel_data.y_position);
    uint16_t width = ntohs(pixel_data.width);
    uint16_t height = ntohs(pixel_data.height);
    LOGGER_DEBUG("(x_position,y_position,width,height)=(%d,%d,%d,%d)",
           x_position, y_position, width, height);
    int32_t encoding_type = pixel_data.encoding_type;
    if (encoding_type != RFB_ENCODING_RAW) {
        LOGGER_DEBUG("unexpected encoding_type:%d", encoding_type);
        return false;
    }
    uint8_t bits_per_pixel = this->pixel_format.bits_per_pixel;
    uint8_t bytes_per_pixel = bits_per_pixel / 8;
    LOGGER_DEBUG("---pixel_format---");
    LOGGER_DEBUG("bits_per_pixel:%d", bits_per_pixel);
    LOGGER_DEBUG("bytes_per_pixel:%d", bytes_per_pixel);
    LOGGER_DEBUG("------------------");

    uint32_t total_pixel_count = width * height;
    uint32_t total_pixel_bytes = total_pixel_count * bits_per_pixel / 8;
    LOGGER_DEBUG("total_pixel_count:%d", total_pixel_count);
    LOGGER_DEBUG("expected total_pixel_bytes:%d", total_pixel_bytes);

    uint32_t total_recv = 0;
    while (total_recv < total_pixel_bytes) {
        memset(buf, 0, sizeof(buf));
        if (total_pixel_bytes - total_recv < sizeof(buf)) {
            recv_length = recv(this->sockfd, buf, total_pixel_bytes - total_recv, 0);
        } else {
            recv_length = recv(this->sockfd, buf, sizeof(buf), 0);
        }
        if (recv_length < 0) {
            return false;
        }
        //LOGGER_DEBUG("recv:%d", recv_length);
        total_recv += recv_length;
        for (int i = 0; i < recv_length; i+=bytes_per_pixel) {
            // uint32_t here is just for container of 4bytes, no need to ntohl()
            uint32_t pixel = 0;
            memmove(&pixel, &buf[i], sizeof(pixel));
            this->image_buf.push_back(pixel);
        }
    }
    LOGGER_DEBUG("total_recv reached total_pixel_bytes:%d", total_recv);
    LOGGER_DEBUG("image_buf size:%d", this->image_buf.size());

    return true;
}

bool vnc_client::recv_colours(uint16_t number_of_colours)
{
    for (int i = 0; i < number_of_colours; i++) {
        LOGGER_DEBUG("--start recv_colours:%d", i + 1);
        if (!this->recv_colour()) {
            LOGGER_DEBUG("failed to recv_colour");
            return false;
        }
        LOGGER_DEBUG("--finish recv_colours:%d", i + 1);
    }
    return true;
}

bool vnc_client::recv_colour()
{
    colour_data_t colour_data = {};

    char buf[BUF_SIZE] = {};
    int recv_length = recv(this->sockfd, buf, sizeof(colour_data), 0);
    if (recv_length < 0) {
        return false;
    }
    LOGGER_DEBUG("recv:%d", recv_length);
    LOGGER_XDEBUG(buf, recv_length);
    return true;
}

bool vnc_client::recv_text(uint32_t length)
{
    char buf[BUF_SIZE] = {};
    int recv_length = recv(this->sockfd, buf, length, 0);
    if (recv_length < 0) {
        return false;
    }
    LOGGER_DEBUG("recv:%d", recv_length);
    LOGGER_DEBUG(buf);
    return true;
}

const uint32_t vnc_client::convert_key_to_code(std::string key)
{
    uint32_t key_code = XStringToKeysym(key.c_str());
    if (key_code != 0) {
        LOGGER_DEBUG("key_code:0x%08lx", key_code);
        return key_code;
    }
    LOGGER_DEBUG("Key not found, try mrhc own correspondence table");
    if (key == vnc_client::KEY_BACKSPACE) {
        key_code = RFB_KEY_CODE_BACKSPACE;
    } else if (key == vnc_client::KEY_PERIOD) {
        key_code = RFB_KEY_CODE_PERIOD;
    } else if (key == vnc_client::KEY_ENTER) {
        key_code = RFB_KEY_CODE_ENTER;
    } else if (key == vnc_client::KEY_SPACE) {
        key_code = RFB_KEY_CODE_SPACE;
    } else if (key == vnc_client::KEY_SLASH) {
        key_code = RFB_KEY_CODE_SLASH;
    } else {
        LOGGER_DEBUG("Failed to detect key_code");
        return 0;
    }
    LOGGER_DEBUG("key_code:0x%08lx", key_code);
    return key_code;
}

bool vnc_client::draw_image()
{
    uint8_t bits_per_pixel   = this->pixel_format.bits_per_pixel;
    uint8_t depth            = this->pixel_format.depth;
    uint8_t big_endian_flag  = this->pixel_format.big_endian_flag;
    uint8_t true_colour_flag = this->pixel_format.true_colour_flag;
    uint16_t red_max         = ntohs(this->pixel_format.red_max);
    uint16_t green_max       = ntohs(this->pixel_format.green_max);
    uint16_t blue_max        = ntohs(this->pixel_format.blue_max);
    uint8_t red_shift        = this->pixel_format.red_shift;
    uint8_t green_shift      = this->pixel_format.green_shift;
    uint8_t blue_shift       = this->pixel_format.blue_shift;

    LOGGER_DEBUG("---pixel_format---");
    LOGGER_DEBUG("bits_per_pixel:%d",   bits_per_pixel);
    LOGGER_DEBUG("depth:%d",            depth);
    LOGGER_DEBUG("big_endian_flag:%d",  big_endian_flag);
    LOGGER_DEBUG("true_colour_flag:%d", true_colour_flag);
    LOGGER_DEBUG("red_max:%d",          red_max);
    LOGGER_DEBUG("green_max:%d",        green_max);
    LOGGER_DEBUG("blue_max:%d",         blue_max);
    LOGGER_DEBUG("red_shift:%d",        red_shift);
    LOGGER_DEBUG("green_shift:%d",      green_shift);
    LOGGER_DEBUG("blue_shift:%d",       blue_shift);
    LOGGER_DEBUG("------------------");

    this->image = cv::Mat(this->height, this->width, CV_8UC3, cv::Scalar(0, 0, 0));
    LOGGER_DEBUG("%dx%d", this->width, this->height);
    for (int y = 0; y < this->height; y++) {
        for (int x = 0; x < this->width; x++) {
            uint32_t pixel = this->image_buf[this->width * y + x];
            uint8_t red = ((pixel >> red_shift) & red_max);
            uint8_t green = ((pixel >> green_shift) & green_max);
            uint8_t blue = ((pixel >> blue_shift) & blue_max);
            //LOGGER_DEBUG("(R,G,B)=(%d,%d,%d)", red, green, blue);
            rectangle(this->image, cv::Point(x, y), cv::Point(x+1, y+1), cv::Scalar(blue, green, red), -1, CV_AA);
        }
    }
    cv::imencode(".jpeg", this->image, this->jpeg_buf);
    return true;
}

bool vnc_client::draw_pointer(uint16_t x, uint16_t y)
{
    if (x == 0 && y == 0) {
        return true;
    }
    for (int d = 0; d < 5; d++) {
        uint16_t left_x = x - d;
        uint16_t right_x = x + d;
        uint16_t upper_y = y - d;
        uint16_t lower_y = y + d;
        rectangle(this->image, cv::Point(left_x, upper_y), cv::Point(left_x+1, upper_y+1), cv::Scalar(0, 0, 0), -1, CV_AA);
        rectangle(this->image, cv::Point(right_x, upper_y), cv::Point(right_x+1, upper_y+1), cv::Scalar(0, 0, 0), -1, CV_AA);
        rectangle(this->image, cv::Point(left_x, lower_y), cv::Point(left_x+1, lower_y+1), cv::Scalar(0, 0, 0), -1, CV_AA);
        rectangle(this->image, cv::Point(right_x, lower_y), cv::Point(right_x+1, lower_y+1), cv::Scalar(0, 0, 0), -1, CV_AA);
    }
    cv::imencode(".jpeg", this->image, this->jpeg_buf);
    return true;
}

void vnc_client::clear_buf()
{
    this->image_buf.clear();
    this->jpeg_buf.clear();
}
