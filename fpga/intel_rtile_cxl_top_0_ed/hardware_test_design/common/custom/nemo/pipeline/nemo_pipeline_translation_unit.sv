import afu_axi_if_pkg::*;
import nemo_defines::*;

module nemo_pipeline_translation_unit #(
     parameter N_DATAPATHS = 2
) (
     input                                  clk
    ,input                                  rst

    ,input  t_nemo_datapath_ch      [N_DATAPATHS-1:0]   nemo_s_in_datapath
    ,input  t_nemo_datapath_valid   [N_DATAPATHS-1:0]   nemo_s_in_datapath_valid
    ,output t_nemo_datapath_ready   [N_DATAPATHS-1:0]   nemo_s_out_datapath_ready

    ,output t_nemo_datapath_ch      [N_DATAPATHS-1:0]   nemo_m_out_datapath
    ,output t_nemo_datapath_valid   [N_DATAPATHS-1:0]   nemo_m_out_datapath_valid
    ,input  t_nemo_datapath_ready   [N_DATAPATHS-1:0]   nemo_m_in_datapath_ready

    ,input  t_nemo_controlpath_req_ch                   nemo_s_in_controlpath_req
    ,input  t_nemo_controlpath_req_valid                nemo_s_in_controlpath_req_valid
    ,output t_nemo_controlpath_req_ready                nemo_s_out_controlpath_req_ready

    ,output t_nemo_controlpath_resp_ch                  nemo_m_out_controlpath_resp
    ,output t_nemo_controlpath_resp_valid               nemo_m_out_controlpath_resp_valid
    ,input  t_nemo_controlpath_resp_ready               nemo_m_in_controlpath_resp_ready
);

// sram maps sram addrs to sram addrs
// top bit is valid bit
logic [N_STATES-1:0][SRAM_LOG_DEPTH:0] sram;

// write logic
logic [N_STATES-1:0] sram_write_addr;
logic sram_write_enabled;
logic [SRAM_LOG_DEPTH:0] sram_write_data;

always @(posedge clk) begin
    if (rst) begin
    end else begin
        if (sram_write_enabled) begin
            sram[sram_write_addr] <= sram_write_data;
        end
    end
end

t_nemo_controlpath_req_ch       nemo_s_in_controlpath_req_postreg;
t_nemo_controlpath_req_valid    nemo_s_in_controlpath_req_postreg_valid;
t_nemo_controlpath_req_ready    nemo_s_out_controlpath_req_postreg_ready;

nemo_controlpath_req_fifo controlpath_req_fifo_inst (
     .clk(clk)
    ,.rst(rst)

    ,.nemo_s_in_controlpath_req (nemo_s_in_controlpath_req)
    ,.nemo_s_in_controlpath_req_valid   (nemo_s_in_controlpath_req_valid)
    ,.nemo_s_out_controlpath_req_ready  (nemo_s_out_controlpath_req_ready)

    ,.nemo_m_out_controlpath_req    (nemo_s_in_controlpath_req_postreg)
    ,.nemo_m_out_controlpath_req_valid  (nemo_s_in_controlpath_req_postreg_valid)
    ,.nemo_m_in_controlpath_req_ready   (nemo_s_out_controlpath_req_postreg_ready)
);

assign sram_write_addr = nemo_s_in_controlpath_req_postreg.pipeline_local_sram_addr;
assign sram_write_enabled = nemo_s_in_controlpath_req_postreg_valid && nemo_s_out_controlpath_req_postreg_ready && nemo_s_in_controlpath_req_postreg.req_metadata.is_write;
assign sram_write_data = nemo_s_in_controlpath_req_postreg.write_data;
assign nemo_s_out_controlpath_req_postreg_ready = nemo_m_in_controlpath_resp_ready;

assign nemo_m_out_controlpath_resp.read_data = '1;
assign nemo_m_out_controlpath_resp.req_metadata = nemo_s_in_controlpath_req_postreg.req_metadata;
assign nemo_m_out_controlpath_resp.resp_ok_n[0] = 0;
assign nemo_m_out_controlpath_resp.resp_ok_n[1] = |nemo_s_in_controlpath_req_postreg.write_data[63:SRAM_LOG_DEPTH+1]; // invalid write data
assign nemo_m_out_controlpath_resp.resp_ok_n[2] = !nemo_s_in_controlpath_req_postreg.req_metadata.is_write;
assign nemo_m_out_controlpath_resp_valid = nemo_s_in_controlpath_req_postreg_valid;

// read / translation logic
generate
for (genvar datapath_i = 0; datapath_i < N_DATAPATHS; datapath_i++) begin
    t_nemo_datapath_ch              post_fifo_datapath;
    t_nemo_datapath_valid           post_fifo_datapath_valid;
    t_nemo_datapath_ready           post_fifo_datapath_ready;

    logic [SRAM_LOG_DEPTH:0] sram_output;
    assign sram_output = sram[post_fifo_datapath.pipeline_local_sram_addr];
    always_comb begin
        nemo_m_out_datapath[datapath_i] = post_fifo_datapath;
        nemo_m_out_datapath[datapath_i].pipeline_local_sram_addr = sram_output[SRAM_LOG_DEPTH-1:0];

        nemo_m_out_datapath_valid[datapath_i] = post_fifo_datapath_valid && sram_output[SRAM_LOG_DEPTH];
        post_fifo_datapath_ready = nemo_m_in_datapath_ready[datapath_i] || !sram_output[SRAM_LOG_DEPTH];
    end

    nemo_datapath_fifo translation_fifo (
         .clk(clk)
        ,.rst(rst)

        ,.nemo_s_in_datapath    (nemo_s_in_datapath[datapath_i])
        ,.nemo_s_in_datapath_valid  (nemo_s_in_datapath_valid[datapath_i])
        ,.nemo_s_out_datapath_ready (nemo_s_out_datapath_ready[datapath_i])

        ,.nemo_m_out_datapath   (post_fifo_datapath)
        ,.nemo_m_out_datapath_valid (post_fifo_datapath_valid)
        ,.nemo_m_in_datapath_ready  (post_fifo_datapath_ready)
    );
end
endgenerate

endmodule
