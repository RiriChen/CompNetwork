#pragma once

#define PROG_MD_CLI     0
#define PROG_MD_SVR     1
#define DEF_PORT_NO     2080
#define FNAME_SZ        150
#define PROG_DEF_FNAME  "test.c"
#define PROG_DEF_SVR_ADDR   "127.0.0.1"

typedef struct prog_config{
    int     prog_mode;
    int     port_number;
    char    svr_ip_addr[16];
    char    file_name[128];
} prog_config;

#define DUFTP_MT_FILE_INFO      1    // Client sends file metadata
#define DUFTP_MT_FILE_INFO_ACK  2    // Server acknowledges file info
#define DUFTP_MT_FILE_DATA      3    // File data transfer
#define DUFTP_MT_FILE_DATA_ACK  4    // Acknowledge data received
#define DUFTP_MT_FILE_COMPLETE  5    // Transfer complete
#define DUFTP_MT_ERROR          6    // Error occurred

#define DUFTP_STATUS_OK         0
#define DUFTP_STATUS_ERORR      1

typedef struct duftp_pdu {
    int     msg_type;                // Message type (DUFTP_MT_*)
    int     status;                  // Status code
    long    file_size;               // Total file size (for FILE_INFO)
    long    bytes_transferred;       // Bytes transferred so far
    char    file_name[FNAME_SZ];     // Name of the file
} duftp_pdu;
