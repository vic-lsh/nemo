import nemo_defines::*;

module nemo_csr_to_nemo_req (

    // AVMM Slave Interface
    // input               clk,
    // input               rst,
    input  logic [63:0] write_data,
    input  logic        read_valid,
    input  logic        write_valid,
    input  logic [63:0] write_mask,
    input  logic [31:0] read_write_address,
    input  logic        read_PF_capability_chain

    ,output nemo_defines::t_nemo_controlpath_req_ch                   controlpath_req
    ,output nemo_defines::t_nemo_controlpath_req_valid                controlpath_req_valid
    ,input  nemo_defines::t_nemo_controlpath_req_ready                controlpath_req_ready
);

assign controlpath_req_valid = write_valid || read_valid;
assign controlpath_req.addr = read_write_address[20:3];
assign controlpath_req.req_metadata.is_write = write_valid;
assign controlpath_req.write_data = write_data;
assign controlpath_req.req_metadata.req_ok_n[0] = read_PF_capability_chain;
assign controlpath_req.req_metadata.req_ok_n[1] = write_valid && (write_mask != 64'hFFFFFFFF_FFFFFFFF);
assign controlpath_req.req_metadata.req_ok_n[2] = write_valid && read_valid;
assign controlpath_req.req_metadata.req_ok_n[3] = read_write_address[21];

assign controlpath_req.req_metadata.cxl_is_source    = 1'b0;
assign controlpath_req.req_metadata.cxl_src_channel  = 0;
assign controlpath_req.req_metadata.cxl_resp_id      = 0;
assign controlpath_req.req_metadata.cxl_resp_last    = 1;

endmodule
