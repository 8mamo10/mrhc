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

#include <vector>

#include "ap_config.h"

#include "mrhc_common.h"
#include "vnc_client.h"

extern "C" module AP_MODULE_DECLARE_DATA mrhc_module;

static apr_status_t ap_get_vnc_param_by_basic_auth_components(const request_rec *r, const char **host, int *port, const char **password);
static std::vector<std::string> split_string(std::string s, std::string delim);

/* The sample content handler */
static int mrhc_handler(request_rec *r)
{
    log_access("called");
    log_error("called");
    log_debug("called");

    if (strcmp(r->handler, "mrhc")) {
        return DECLINED;
    }
    r->content_type = "text/html";

    if (r->header_only) {
        return DECLINED;
    }

    const char *host;
    int port = 0;
    const char *password;
    //apr_status_t ret = ap_get_basic_auth_components(r, &username, &password);
    apr_status_t ret = ap_get_vnc_param_by_basic_auth_components(r, &host, &port, &password);
    if (ret == APR_EINVAL) {
        apr_table_set(r->err_headers_out, "WWW-Authenticate", "Basic real=\"\"");
        return HTTP_UNAUTHORIZED;
    }
    if (ret == APR_SUCCESS) {

        //ap_rputs("---Start VNC Client", r);

        //ap_rputs("<br/>", r);
        //ap_rputs(host, r);
        //ap_rputs("<br/>", r);
        //ap_rputs(std::to_string(port).c_str(), r);
        //ap_rputs("<br/>", r);
        //ap_rputs(password, r);
        //ap_rputs("<br/>", r);

        vnc_client *client = new vnc_client(host, port, password);
        if (!client->connect_to_server()) {
            //ap_rputs("Failed to connect_to_server.", r);
            return OK;
        }

        //ap_rputs("---Connected", r);
        //ap_rputs("<br/>", r);

        if (!client->exchange_protocol_version()) {
            //ap_rputs("Failed to exchange_protocol_version.", r);
            return OK;
        }

        //ap_rputs("---Exchanged protocol version", r);
        //ap_rputs("<br/>", r);

        if (!client->exchange_security_type()) {
            //ap_rputs("Failed to exchange_security_type.", r);
            return OK;
        }

        //ap_rputs("---Exchanged security type", r);
        //ap_rputs("<br/>", r);

        if (!client->vnc_authentication()) {
            //ap_rputs("Failed to vnc_authentication.", r);
            return OK;
        }

        //ap_rputs("---VNC authenticated", r);
        //ap_rputs("<br/>", r);

        if (!client->exchange_init()) {
            //ap_rputs("Failed to exchange_init.", r);
            return OK;
        }

        //ap_rputs("---Exchanged Client/Server Init", r);
        //ap_rputs("<br/>", r);

        if (!client->frame_buffer_update()) {
            //ap_rputs("Failed to frame_buffer_update.", r);
            return OK;
        }
        std::vector<uint8_t> image_buf = client->get_image_buf();
        //ap_rputs("size of image_buf=", r);
        //ap_rputs(std::to_string(image_buf.size()).c_str(), r);
        //ap_rputs("\n", r);
        r->content_type = "image/jpeg";
        char jpeg[1024*768] = {};
        for (unsigned int i = 0; i < image_buf.size(); i++) {
            jpeg[i] = image_buf[i];
        }
        ap_rwrite(jpeg, image_buf.size(), r);
        return OK;
    }
    ap_rputs("not reach here\n", r);
    return OK;
}

// Since I want to use `:` as a part of username of basic auth, I reinvent a function.
// see: httpd-2.4.41/server/protocol.c
static apr_status_t ap_get_vnc_param_by_basic_auth_components(const request_rec *r, const char **host, int *port, const char **password)
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

    // vnc host is to be like 192.168.1.10:5900.
    //user = ap_getword_nulls(r->pool, &decoded, ':');
    std::vector<std::string> vnc_params = split_string(decoded, ":");

    if (host) {
        *host = vnc_params[0].c_str();
    }
    if (port) {
        *port = stoi(vnc_params[1]);
    }
    if (password) {
        *password = vnc_params[2].c_str();
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
