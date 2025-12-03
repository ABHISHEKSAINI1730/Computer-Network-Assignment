// udp_calc_server.c
// Compile: gcc udp_calc_server.c -o udp_calc_server -lm
// Run: sudo ./udp_calc_server <port>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF 1024

// Trim leading/trailing whitespace
char *trim(char *s) {
    while(*s==' '||*s=='\t'||*s=='\n' ) s++;
    char *end = s + strlen(s) - 1;
    while(end>s && (*end==' '||*end=='\t'||*end=='\n')) { *end=0; end--; }
    return s;
}

int parse_double(const char *s, double *out) {
    char *end;
    double v = strtod(s, &end);
    if (end==s) return 0;
    *out = v;
    return 1;
}

int starts_with(const char *s, const char *p) {
    return strncasecmp(s, p, strlen(p))==0;
}

// Evaluate expression. Returns 1 if ok and result in res, else 0 with errmsg.
int eval_expr(const char *expr, double *res, char *errmsg, size_t emsglen) {
    char tmp[BUF];
    strncpy(tmp, expr, sizeof(tmp)-1); tmp[sizeof(tmp)-1]=0;
    char *s = trim(tmp);

    // unary functions: sin x  OR sin(x)
    if (starts_with(s, "sin")) {
        char *arg = s + 3;
        if (*arg == '(') { arg++; char *p = strchr(arg, ')'); if (p) *p=0; }
        arg = trim(arg);
        double x;
        if (!parse_double(arg, &x)) { snprintf(errmsg, emsglen, "bad arg to sin"); return 0; }
        *res = sin(x); return 1;
    }
    if (starts_with(s, "cos")) {
        char *arg = s + 3; if (*arg == '(') { arg++; char *p = strchr(arg, ')'); if (p) *p=0; }
        arg = trim(arg);
        double x; if (!parse_double(arg, &x)) { snprintf(errmsg, emsglen, "bad arg to cos"); return 0; }
        *res = cos(x); return 1;
    }
    if (starts_with(s, "tan")) {
        char *arg = s + 3; if (*arg == '(') { arg++; char *p = strchr(arg, ')'); if (p) *p=0; }
        arg = trim(arg);
        double x; if (!parse_double(arg, &x)) { snprintf(errmsg, emsglen, "bad arg to tan"); return 0; }
        *res = tan(x); return 1;
    }
    if (starts_with(s, "inv")) { // inv x  => 1/x
        char *arg = s + 3; if (*arg == '(') { arg++; char *p = strchr(arg, ')'); if (p) *p=0; }
        arg = trim(arg);
        double x; if (!parse_double(arg, &x)) { snprintf(errmsg, emsglen, "bad arg to inv"); return 0; }
        if (x == 0) { snprintf(errmsg, emsglen, "divide by zero"); return 0; }
        *res = 1.0/x; return 1;
    }
    if (starts_with(s, "sqrt")) {
        char *arg = s + 4; if (*arg == '(') { arg++; char *p = strchr(arg, ')'); if (p) *p=0; }
        arg = trim(arg);
        double x; if (!parse_double(arg, &x)) { snprintf(errmsg, emsglen, "bad arg to sqrt"); return 0; }
        if (x < 0) { snprintf(errmsg, emsglen, "sqrt of negative"); return 0; }
        *res = sqrt(x); return 1;
    }

    // binary operations: try to find + - * /
    // locate operator (left->right preferred for +,-,* ,/)
    char *op = NULL;
    char *plus = strchr(s, '+');
    char *minus = strchr(s, '-');
    char *mul = strchr(s, '*');
    char *divi = strchr(s, '/');

    // prefer first-occurring operator (but handle negative numbers by checking position)
    // find operator that is not first char (to avoid unary minus)
    char *candidates[] = {plus, minus, mul, divi};
    char ops[] = {'+', '-', '*', '/'};
    for (int i=0;i<4;i++) {
        char *p = candidates[i];
        if (p && p != s) { op = p; break; }
    }

    if (op) {
        char left[256], right[256];
        strncpy(left, s, op - s);
        left[op - s] = 0;
        strcpy(right, op+1);
        char *L = trim(left);
        char *R = trim(right);
        double a,b;
        if (!parse_double(L, &a)) { snprintf(errmsg, emsglen, "bad left operand"); return 0; }
        if (!parse_double(R, &b)) { snprintf(errmsg, emsglen, "bad right operand"); return 0; }
        switch(*op) {
            case '+': *res = a + b; return 1;
            case '-': *res = a - b; return 1;
            case '*': *res = a * b; return 1;
            case '/': if (b==0) { snprintf(errmsg, emsglen, "divide by zero"); return 0; } *res = a / b; return 1;
        }
    }

    // maybe single number
    double v;
    if (parse_double(s, &v)) { *res = v; return 1; }
    snprintf(errmsg, emsglen, "unsupported expression");
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) { printf("Usage: %s <port>\n", argv[0]); return 1; }
    int port = atoi(argv[1]);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in serv, cli;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = INADDR_ANY;
    serv.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&serv, sizeof(serv)) < 0) { perror("bind"); return 1; }

    printf("UDP Calculator server listening on port %d\n", port);

    char buf[BUF];
    socklen_t cli_len = sizeof(cli);
    while (1) {
        ssize_t r = recvfrom(sock, buf, sizeof(buf)-1, 0, (struct sockaddr*)&cli, &cli_len);
        if (r < 0) continue;
        buf[r] = 0;

        // Expect messages: ID|expression
        char id[64], expr[512];
        char *pipe = strchr(buf, '|');
        if (!pipe) {
            // malformed
            char resp[BUF];
            snprintf(resp, sizeof(resp), "0|ERR|malformed request");
            sendto(sock, resp, strlen(resp), 0, (struct sockaddr*)&cli, cli_len);
            continue;
        }
        int idlen = pipe - buf;
        strncpy(id, buf, idlen); id[idlen]=0;
        strncpy(expr, pipe+1, sizeof(expr)-1); expr[sizeof(expr)-1]=0;
        char *e = trim(expr);

        double result;
        char errmsg[128];
        if (eval_expr(e, &result, errmsg, sizeof(errmsg))) {
            char out[BUF];
            // send ID|OK|<result>
            snprintf(out, sizeof(out), "%s|OK|%.8g", id, result);
            sendto(sock, out, strlen(out), 0, (struct sockaddr*)&cli, cli_len);
        } else {
            char out[BUF];
            snprintf(out, sizeof(out), "%s|ERR|%s", id, errmsg);
            sendto(sock, out, strlen(out), 0, (struct sockaddr*)&cli, cli_len);
        }
    }

    close(sock);
    return 0;
}
