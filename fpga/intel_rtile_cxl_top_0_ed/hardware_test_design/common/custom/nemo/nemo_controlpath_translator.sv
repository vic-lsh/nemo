import nemo_defines::*;

module nemo_controlpath_translator #(
     parameter SYSTEM_BIT = 18
) (
    //  input clk
    // ,input rst

     input  t_nemo_controlpath_req_ch      nemo_s_in_controlpath_req
    ,input  t_nemo_controlpath_req_valid   nemo_s_in_controlpath_req_valid
    ,output t_nemo_controlpath_req_ready   nemo_s_out_controlpath_req_ready

    ,output t_nemo_controlpath_req_ch      nemo_m_out_controlpath_req
    ,output t_nemo_controlpath_req_valid   nemo_m_out_controlpath_req_valid
    ,input  t_nemo_controlpath_req_ready   nemo_m_in_controlpath_req_ready

    ,input [63:3] pipeline_selector
);

assign nemo_m_out_controlpath_req_valid = nemo_s_in_controlpath_req_valid;
assign nemo_s_out_controlpath_req_ready = nemo_m_in_controlpath_req_ready;

logic [63:0] controlpath_address_offset;
localparam DISABLE = 0;

always_comb begin
    nemo_m_out_controlpath_req = nemo_s_in_controlpath_req;
    nemo_m_out_controlpath_req.addr = '0;
    nemo_m_out_controlpath_req.pipeline_local_sram_addr = '0;
    controlpath_address_offset = 0;
    if (nemo_s_in_controlpath_req.req_metadata.cxl_is_source) begin
        // request came from cxl, don't do translation
        nemo_m_out_controlpath_req.pipeline_local_sram_addr = nemo_s_in_controlpath_req.pipeline_local_sram_addr;
        nemo_m_out_controlpath_req.addr = nemo_s_in_controlpath_req.addr;
    end else begin
        if (DISABLE) begin
            nemo_m_out_controlpath_req.pipeline_local_sram_addr = nemo_s_in_controlpath_req.addr[SYSTEM_BIT-1:3];
        end else begin
            nemo_m_out_controlpath_req.pipeline_local_sram_addr = nemo_s_in_controlpath_req.addr[SYSTEM_BIT-1:3];
            nemo_m_out_controlpath_req.addr[20:3] = pipeline_selector[20:3];
        end
    end
end

endmodule

module nemo_cxl_controlpath_translator (
    //  input clk
    // ,input rst

     input  t_nemo_controlpath_req_ch      nemo_s_in_controlpath_req
    ,input  t_nemo_controlpath_req_valid   nemo_s_in_controlpath_req_valid
    ,output t_nemo_controlpath_req_ready   nemo_s_out_controlpath_req_ready

    ,output t_nemo_controlpath_req_ch      nemo_m_out_controlpath_req
    ,output t_nemo_controlpath_req_valid   nemo_m_out_controlpath_req_valid
    ,input  t_nemo_controlpath_req_ready   nemo_m_in_controlpath_req_ready

    ,input [63:0] controlpath_start_physical_address
);

assign nemo_m_out_controlpath_req_valid = nemo_s_in_controlpath_req_valid;
assign nemo_s_out_controlpath_req_ready = nemo_m_in_controlpath_req_ready;

logic [63:0] controlpath_address_offset;

always_comb begin
    nemo_m_out_controlpath_req = nemo_s_in_controlpath_req;
    nemo_m_out_controlpath_req.addr = '0;
    nemo_m_out_controlpath_req.pipeline_local_sram_addr = '0;
    controlpath_address_offset = 0;
    if (nemo_s_in_controlpath_req.req_metadata.cxl_is_source) begin
        controlpath_address_offset = {nemo_s_in_controlpath_req.addr, 3'b0} - controlpath_start_physical_address;
        // request came from cxl, split address into appropriate parts
        nemo_m_out_controlpath_req.pipeline_local_sram_addr = controlpath_address_offset[SRAM_LOG_DEPTH-1+6:6]; // can't go sub-cacheline in cxl
        nemo_m_out_controlpath_req.addr[20:3] = controlpath_address_offset[SRAM_LOG_DEPTH+6+17:SRAM_LOG_DEPTH+6];
    end else begin
        // do nothing, let the above module handle this after demux
        nemo_m_out_controlpath_req.pipeline_local_sram_addr = nemo_s_in_controlpath_req.pipeline_local_sram_addr;
        nemo_m_out_controlpath_req.addr = nemo_s_in_controlpath_req.addr;
    end
end

endmodule
