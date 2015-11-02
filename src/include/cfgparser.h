#ifndef CFGPARSER_H_
#define CFGPARSER_H_

struct pw_config_info {
    enum { PW_WAIT_THRESHOLD, PW_PROCESS_NAME, PW_END_FILE } type;
    union {
        unsigned   wait_threshold;
        const char *process_name;
    } data;
};

struct pw_config_info config_parse(int argc, char **argv);
#endif
