package nemo_defines;

import cxlip_top_pkg::*;
import afu_axi_if_pkg::*;

localparam CHANNEL_W = MC_CHANNEL==1? 1 : $clog2(MC_CHANNEL);
localparam MAX_TRANSACTION_BYTES = 4096;
localparam TRANSACTION_BYTES_W = $clog2(MAX_TRANSACTION_BYTES+1); // must be able to represent 4096

localparam STATE_SIZE_BITS = 64; // to match CSR
localparam SRAM_STATE_INTERNAL_W = $clog2(STATE_SIZE_BITS);

localparam RAM_BYTES = 64 * 1024; // 64 KiB
localparam N_STATES = RAM_BYTES*8 / STATE_SIZE_BITS; // 8192 states, one per 2MiB region in 16GiB
localparam SRAM_LOG_DEPTH = $clog2(N_STATES);

// could add to this if we ever find that any other aw or ar things are useful for telem.
typedef struct packed {
    logic   [CHANNEL_W-1:0]                 src_channel;
    logic                                   is_write;
    logic   [AFU_AXI_MAX_ADDR_WIDTH-1:0]    addr;
    logic   [TRANSACTION_BYTES_W-1:0]       transaction_bytes;
    logic   [SRAM_LOG_DEPTH-1:0]               pipeline_local_sram_addr;
    logic   [SRAM_STATE_INTERNAL_W-1:0]     sram_internal_addr;
} t_nemo_datapath_ch;

typedef logic t_nemo_datapath_valid;
typedef logic t_nemo_datapath_ready;
localparam NEMO_DATAPATH_WIDTH = $bits(t_nemo_datapath_ch);

localparam N_REQ_ERRS = 4;
localparam N_RESP_ERRS = 3;

typedef struct packed {
    logic is_write;
    logic [N_REQ_ERRS-1:0]req_ok_n; // ok == 0. bit 0 = was a config access, bit 1 = had a write mask. bit 2 = both read and write. bit 3 = OOB (bit 21 set)

    // stuff to forward to AXI
    logic                                   cxl_is_source;
    logic [CHANNEL_W-1:0]                   cxl_src_channel;
    logic                                   cxl_resp_last;
    logic [AFU_AXI_MAX_ID_WIDTH-1:0]        cxl_resp_id;
} t_nemo_controlpath_req_metadata;

// currently assumed to be a read to addr of size 1 cacheline = 64 bytes.
typedef struct packed {
    logic   [63:3]    addr;
    logic   [63:0]    write_data;

    logic   [SRAM_LOG_DEPTH-1:0]     pipeline_local_sram_addr; // set by the pipeline selector + translation unit, leave undefined to start

    // things to forward to response
    t_nemo_controlpath_req_metadata req_metadata;


} t_nemo_controlpath_req_ch;

typedef logic t_nemo_controlpath_req_valid;
typedef logic t_nemo_controlpath_req_ready;
localparam NEMO_CONTROLPATH_REQ_WIDTH = $bits(t_nemo_controlpath_req_ch);

typedef struct packed {
    logic [63:0]      read_data;

    logic [N_RESP_ERRS-1:0]resp_ok_n; // bit 0 = invalid address, bit 1 = invalid data, bit 2 = reads not supported

    t_nemo_controlpath_req_metadata req_metadata;

} t_nemo_controlpath_resp_ch;

typedef logic t_nemo_controlpath_resp_valid;
typedef logic t_nemo_controlpath_resp_ready;
localparam NEMO_CONTROLPATH_RESP_WIDTH = $bits(t_nemo_controlpath_resp_ch);

endpackage
