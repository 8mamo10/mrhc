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

#include <bits/stdc++.h>
#include <vector>

#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "ap_config.h"

#include "vnc_client.h"

extern "C" module AP_MODULE_DECLARE_DATA mrhc_module;

using namespace std;

static apr_status_t ap_get_vnc_param_by_basic_auth_components(const request_rec *r, const char **host, int *port, const char **password);
static vector<string> split_string(string s, string delim);

/* The sample content handler */
static int mrhc_handler(request_rec *r)
{
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

        VncClient *client = new VncClient(host, port, password);
        if (!client->connectToServer()) {
            ap_rputs("Failed to connect", r);
            return OK;
        }

        ap_rputs(host, r);
        ap_rputs("<br/>", r);
        ap_rputs(to_string(port).c_str(), r);
        ap_rputs("<br/>", r);
        ap_rputs(password, r);
        ap_rputs("<br/>", r);

        //ap_rputs(password, r);
        // ap_rputs(client->parseHost().c_str(), r);
        // ap_rputs("<br/>", r);
        // ap_rputs(client->parsePort().c_str(), r);
        /*
        char str[1024];
        sprintf(str, "username: %s", username);
        ap_rputs(str, r);
        ap_rputs("<br/>", r);
        sprintf(str, "password: %s", password);
        ap_rputs(str, r);
        ap_rputs("<br/>", r);

        string ip_addr = username;
        int port = stoi(string(password));

        // socket
        int sockfd;
        struct sockaddr_in addr;
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            ap_rputs("failed to open socket", r);
            return OK;
        }
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip_addr.c_str());

        connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

        // recv version
        char recv_str[1024];
        int recv_len = recv(sockfd, recv_str, 1024, 0);
        ap_rputs("recv_len=", r);
        ap_rputs(to_string(recv_len).c_str(), r);
        ap_rputs("<br/>", r);

        ap_rputs("recv_str=", r);
        ap_rputs(string(recv_str).substr(0, recv_len).c_str(), r);
        ap_rputs("<br/>", r);

        // send version
        int send_len = send(sockfd, recv_str, recv_len, 0);
        ap_rputs("send_len=", r);
        ap_rputs(to_string(send_len).c_str(), r);
        ap_rputs("<br/>", r);

        // recv security
        char recv_str2[1024];
        int recv_len2 = recv(sockfd, recv_str2, 1024, 0);
        ap_rputs("recv_len2=", r);
        ap_rputs(to_string(recv_len2).c_str(), r);
        ap_rputs("<br/>", r);

        ap_rputs("recv_str2=", r);
        ap_rputs(string(recv_str2).substr(0, recv_len2).c_str(), r);
        ap_rputs("<br/>", r);

        ap_rputs("recv_str2(hex)=", r);
        for (int i=0; i < recv_len2; i++) {
            char x[256];
            sprintf(x, "0x%02X,", recv_str2[i]);
            ap_rputs(x, r);
        }
        ap_rputs("<br/>", r);

        // send security
        int securityType = 0x02;
        int send_len2 = send(sockfd, &securityType, 1, 0);
        ap_rputs("send_len2=", r);
        ap_rputs(to_string(send_len2).c_str(), r);
        ap_rputs("<br/>", r);

        // recv vnc auth
        char recv_str3[1024];
        int recv_len3 = recv(sockfd, recv_str3, 1024, 0);
        ap_rputs("recv_len3=", r);
        ap_rputs(to_string(recv_len3).c_str(), r);
        ap_rputs("<br/>", r);

        ap_rputs("recv_str3=", r);
        ap_rputs(string(recv_str3).substr(0, recv_len3).c_str(), r);
        ap_rputs("<br/>", r);

        close(sockfd);
        */
        return OK;
    }
    ap_rputs("not reach here\n", r);
    return OK;
}

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
    vector<string> vnc_params = split_string(decoded, ":");

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

static vector<string> split_string(string s, string delim)
{
    vector<string> v;
    while (true) {
        size_t i = s.find_first_of(delim);
        if (i == string::npos) {
            v.push_back(s);
            break;
        }
        string item = s.substr(0, i);
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
