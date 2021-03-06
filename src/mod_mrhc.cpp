/* 
**  mod_mrhc.c -- Apache sample mrhc module
**  [Autogenerated via ``apxs -n mrhc -g'']
**
**  To play with this sample module first compile it into a
**  DSO file and install it into Apache's modules directory 
**  by running:
**
**    $ apxs -c -i mod_mrhc.c
**
**  Then activate it in Apache's httpd.conf file for instance
**  for the URL /mrhc in as follows:
**
**    #   httpd.conf
**    LoadModule mrhc_module modules/mod_mrhc.so
**    <Location /mrhc>
**    SetHandler mrhc
**    </Location>
**
**  Then after restarting Apache via
**
**    $ apachectl restart
**
**  you immediately can request the URL /mrhc and watch for the
**  output of this module. This can be achieved for instance via:
**
**    $ lynx -mime_header http://localhost/mrhc 
**
**  The output should be similar to the following one:
**
**    HTTP/1.1 200 OK
**    Date: Tue, 31 Mar 1998 14:42:22 GMT
**    Server: Apache/1.3.4 (Unix)
**    Connection: close
**    Content-Type: text/html
**  
**    The sample page from mod_mrhc.c
*/ 

#include <unistd.h>
#include <vector>

#include "ap_config.h"

#include "mrhc_common.h"
#include "vnc_client.h"

extern "C" module AP_MODULE_DECLARE_DATA mrhc_module;

static bool mrhc_spin(vnc_client *client, request_rec *r);
static bool mrhc_confirm(request_rec *r);
static bool mrhc_throw(vnc_client *client, request_rec *r);
static const vnc_operation_t mrhc_query(const request_rec *r);
static const std::string mrhc_html(const request_rec *r, const vnc_client *client);
static const std::string mrhc_error(const request_rec *r, const std::string message);
static apr_status_t ap_get_vnc_param_by_basic_auth_components(const request_rec *r, char *host, int *port, char *password);
static std::vector<std::string> split_string(std::string s, std::string delim);

// TODO: Need to support multi process but only support single process for now
vnc_client *client_cache = NULL;

/* The sample content handler */
static int mrhc_handler(request_rec *r)
{
    LOGGER_ACCESS("called");
    LOGGER_ERROR("called");
    LOGGER_DEBUG("called");

    if (strcmp(r->handler, "mrhc")) {
        return DECLINED;
    }

    if (r->header_only) {
        return DECLINED;
    }

    if (client_cache == NULL) {
        // get the throwing destination with basic authentication
        char host[BUF_SIZE] = {};
        int  port = 0;
        char password[BUF_SIZE] = {};
        apr_status_t ret = ap_get_vnc_param_by_basic_auth_components(r, host, &port, password);
        if (ret == APR_EINVAL) {
            apr_table_set(r->err_headers_out, "WWW-Authenticate", "Basic real=\"\"");
            return HTTP_UNAUTHORIZED;
        }
        // mrhc begins to spin
        if (ret == APR_SUCCESS) {
            LOGGER_DEBUG("host:%s", host);
            LOGGER_DEBUG("port:%d", port);
            LOGGER_DEBUG("password:%s", password);

            LOGGER_DEBUG("Start VNC Client");
            vnc_client *client = new vnc_client(host, port, password);
            if (!mrhc_spin(client, r)) {
                ap_rputs(mrhc_error(r, "failed to mrhc, please try again.").c_str(), r);
            }
            client_cache = client;
            return OK;
        }
        // mrhc is disqualified
        ap_rputs(mrhc_error(r, "fatal error").c_str(), r);
        return OK;
    }

    if (!mrhc_confirm(r)) {
        // mrhc cnacels this throwing
        if (client_cache != NULL) {
            delete client_cache;
            client_cache = NULL;
        }
        apr_table_set(r->err_headers_out, "WWW-Authenticate", "Basic real=\"\"");
        return HTTP_UNAUTHORIZED;
    }
    // mrhc is already spinning, ready to throw it.
    LOGGER_DEBUG("VNC Client is already running.");

    if (!mrhc_throw(client_cache, r)) {
        apr_table_set(r->err_headers_out, "WWW-Authenticate", "Basic real=\"\"");
        return HTTP_UNAUTHORIZED;
    }
    return OK;
}

static bool mrhc_spin(vnc_client *client, request_rec *r)
{
    if (client == NULL || r == NULL) {
        LOGGER_DEBUG("Invalid arguments.");
        return false;
    }
    if (!client->initialize()) {
        LOGGER_DEBUG("Failed to initialize.");
        return false;
    }
    if (!client->authenticate()) {
        LOGGER_DEBUG("Failed to authenticate.");
        return false;
    }
    if (!client->configure()) {
        LOGGER_DEBUG("Failed to configure.");
        return false;
    }
    // return initial html page with url and image size
    r->content_type = "text/html";
    ap_rputs(mrhc_html(r, client).c_str(), r);
    return true;
}

static bool mrhc_confirm(request_rec *r)
{
    apr_array_header_t *pairs = NULL;
    int res = ap_parse_form_data(r, NULL, &pairs, -1, BUF_SIZE);
    if (res != OK) {
        LOGGER_DEBUG("failed to ap_parse_form_data.");
        return false;
    }
    while (pairs && !apr_is_empty_array(pairs)) {
        ap_form_pair_t *pair = (ap_form_pair_t *) apr_array_pop(pairs);
        if (strcmp(pair->name, "logout") == 0) {
            LOGGER_DEBUG("do logout.");
            return false;
        }
    }
    return true;
}

static bool mrhc_throw(vnc_client *client, request_rec *r)
{
    if (client == NULL || r == NULL) {
        LOGGER_DEBUG("Invalid arguments.");
        return false;
    }
    vnc_operation_t operation = vnc_operation_t{};
    if (r->parsed_uri.query) {
        operation = mrhc_query(r);
        if (!client->operate(operation)) {
            LOGGER_DEBUG("Failed to operate.");
            return false;
        }
        // wait for the operation to be reflected
        sleep(1);
    }
    if (!client->capture(operation)) {
        LOGGER_DEBUG("Failed to capture.");
        return false;
    }
    std::vector<uint8_t> jpeg_buf = client->get_jpeg_buf();
    LOGGER_DEBUG("jpeg size:%d",jpeg_buf.size());
    char jpeg[jpeg_buf.size()] = {};
    for (unsigned int i = 0; i < jpeg_buf.size(); i++) {
        jpeg[i] = jpeg_buf[i];
    }
    r->content_type = "image/jpeg";
    ap_rwrite(jpeg, jpeg_buf.size(), r);
    return true;
}

static const vnc_operation_t mrhc_query(const request_rec *r)
{
    vnc_operation_t op = vnc_operation_t{};
    if (r->parsed_uri.query == NULL) {
        return op;
    }
    std::vector<std::string> pointer_params = split_string(r->parsed_uri.query, "&");
    for (unsigned int i = 0; i < pointer_params.size(); i++) {
        LOGGER_DEBUG(pointer_params[i]);
        std::vector<std::string> params = split_string(pointer_params[i], "=");
        if (params[0] == std::string("x")) op.x = stoi(params[1]);
        if (params[0] == std::string("y")) op.y = stoi(params[1]);
        if (params[0] == std::string("b")) op.button = stoi(params[1]);
        // @TODO
        if (params[0] == std::string("k")) op.key = params[1].empty() ? vnc_client::KEY_SPACE : params[1];
    }
    return op;
}

static const std::string mrhc_html(const request_rec *r, const vnc_client *client)
{
    std::string html = "";
    if (r == NULL || client == NULL) {
        return html;
    }
    std::string hostname = r->hostname;
    std::string path = r->unparsed_uri;
    std::string width = std::to_string(client->get_width());
    std::string height = std::to_string(client->get_height());
    html ="\
<html>                                                                  \
  <head>                                                                \
    <link rel='shortcut icon' href='https://user-images.githubusercontent.com/562105/83327371-2c590300-a2b6-11ea-90cf-07a5a586f5ae.png'> \
  </head>                                                               \
  <body>                                                                \
    <form action='" + path + "' method='post'>                          \
      <input type='hidden' name='logout' value='1'>                     \
      <input type='submit' value='logout'>                              \
    </form>                                                             \
    <image id='mrhc' src='http://" + hostname + path + "' width='" + width + "' height='" + height + "'> \
  </body>                                                               \
  <script src='https://ajax.googleapis.com/ajax/libs/jquery/3.4.1/jquery.min.js'></script> \
  <script type=text/javascript>                                         \
    let fetchLatestImage = () => {                                      \
      $('#mrhc').attr('src', 'http://" + hostname + path + "?t=' + Date.now()); \
    };                                                                  \
    let timer = setInterval(fetchLatestImage, 5000);                    \
    $('#mrhc').on('click', (e) => {                                     \
      $('#mrhc').attr('src', 'http://" + hostname + path + "?x=' + e.offsetX + '&y=' + e.offsetY + '&b=0'); \
      clearInterval(timer);                                             \
      timer = setInterval(fetchLatestImage, 5000);                      \
    }).on('contextmenu', (e) => {                                       \
      $('#mrhc').attr('src', 'http://" + hostname + path + "?x=' + e.offsetX + '&y=' + e.offsetY + '&b=2'); \
      clearInterval(timer);                                             \
      timer = setInterval(fetchLatestImage, 5000);                      \
      return false;                                                     \
    });                                                                 \
    $(window).on('keydown', (e) => {                                    \
      $.ajax(                                                           \
        {                                                               \
          type: 'GET',                                                  \
          url: 'http://" + hostname + path + "?k=' + e.key,             \
        }                                                               \
      );                                                                \
    });                                                                 \
  </script>                                                             \
</html>";
    LOGGER_DEBUG(html);
    return html;
}

static const std::string mrhc_error(const request_rec *r, const std::string message)
{
    std::string html = "";
    if (r == NULL) {
        return html;
    }
    std::string hostname = r->hostname;
    std::string path = r->unparsed_uri;
    html ="\
<html>                                                                  \
  <body>                                                                \
    <form action='" + path + "' method='post'>                          \
      <input type='hidden' name='logout' value='1'>                     \
      <input type='submit' value='retry'>                               \
    </form>                                                             \
    <p>" + message + "</p>                                              \
  </body>                                                               \
</html>";
    LOGGER_DEBUG(html);
    return html;
}

// Since I want to use `:` as a part of username of basic auth, I reinvent a function.
// see: httpd-2.4.41/server/protocol.c
static apr_status_t ap_get_vnc_param_by_basic_auth_components(const request_rec *r, char *host, int *port, char *password)
{
    const char *auth_header;
    const char *credentials;
    const char *decoded;
    //const char *user;

    auth_header = (PROXYREQ_PROXY == r->proxyreq) ? "Proxy-Authorization"
                                                  : "Authorization";
    credentials = apr_table_get(r->headers_in, auth_header);

    if (!credentials) {
        /* No auth header. */
        return APR_EINVAL;
    }

    if (ap_cstr_casecmp(ap_getword(r->pool, &credentials, ' '), "Basic")) {
        /* These aren't Basic credentials. */
        return APR_EINVAL;
    }

    while (*credentials == ' ' || *credentials == '\t') {
        credentials++;
    }

    /* XXX Our base64 decoding functions don't actually error out if the string
     * we give it isn't base64; they'll just silently stop and hand us whatever
     * they've parsed up to that point.
     *
     * Since this function is supposed to be a drop-in replacement for the
     * deprecated ap_get_basic_auth_pw(), don't fix this for 2.4.x.
     */
    decoded = ap_pbase64decode(r->pool, credentials);
    LOGGER_DEBUG(decoded);

    // vnc host is to be like 192.168.1.10:5900.
    //user = ap_getword_nulls(r->pool, &decoded, ':');
    std::vector<std::string> vnc_params = split_string(decoded, ":");

    if (host) {
        memmove(host, vnc_params[0].c_str(), vnc_params[0].size());
        LOGGER_DEBUG("host:%s", host);
    }
    if (port) {
        try {
            *port = stoi(vnc_params[1]);
        } catch (std::invalid_argument) {
            LOGGER_DEBUG("invalid port: %s", vnc_params[1]);
            return APR_EINVAL;
        }
        LOGGER_DEBUG("port:%d", *port);
    }
    if (password) {
        memmove(password, vnc_params[2].c_str(), vnc_params[2].size());
        LOGGER_DEBUG("password:%s", password);
    }

    return APR_SUCCESS;
}

static std::vector<std::string> split_string(std::string s, std::string delim)
{
    std::vector<std::string> v;
    while (true) {
        size_t i = s.find_first_of(delim);
        if (i == std::string::npos) {
            v.push_back(s);
            break;
        }
        std::string item = s.substr(0, i);
        v.push_back(item);
        s = s.substr(i+1, s.size());
    }
    return v;
}

static void mrhc_register_hooks(apr_pool_t *p)
{
    ap_hook_handler(mrhc_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

extern "C" {
    /* Dispatch list for API hooks */
    module AP_MODULE_DECLARE_DATA mrhc_module = {
        STANDARD20_MODULE_STUFF,
        NULL,                  /* create per-dir    config structures */
        NULL,                  /* merge  per-dir    config structures */
        NULL,                  /* create per-server config structures */
        NULL,                  /* merge  per-server config structures */
        NULL,                  /* table of config file commands       */
        mrhc_register_hooks  /* register hooks                      */
    };
};
