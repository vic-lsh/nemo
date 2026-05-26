import afu_axi_if_pkg::*;
import nemo_defines::*;

module nemo_controlpath_demux #(
    parameter END_BIT = 0,
    parameter START_BIT = 0,
    localparam NUM_BITS = END_BIT - START_BIT + 1,
    localparam N_OUTPUTS = 1 << NUM_BITS
) (
     input                                  clk
    ,input                                  rst

    ,input  t_nemo_controlpath_req_ch       nemo_s_in_controlpath_req
    ,input  t_nemo_controlpath_req_valid    nemo_s_in_controlpath_req_valid
    ,output t_nemo_controlpath_req_ready    nemo_s_out_controlpath_req_ready

    ,output t_nemo_controlpath_req_ch       [N_OUTPUTS-1:0] nemo_m_out_controlpath_req
    ,output t_nemo_controlpath_req_valid    [N_OUTPUTS-1:0] nemo_m_out_controlpath_req_valid
    ,input  t_nemo_controlpath_req_ready    [N_OUTPUTS-1:0] nemo_m_in_controlpath_req_ready

    ,input  t_nemo_controlpath_resp_ch      [N_OUTPUTS-1:0] nemo_s_in_controlpath_resp
    ,input  t_nemo_controlpath_resp_valid   [N_OUTPUTS-1:0] nemo_s_in_controlpath_resp_valid
    ,output t_nemo_controlpath_resp_ready   [N_OUTPUTS-1:0] nemo_s_out_controlpath_resp_ready

    ,output t_nemo_controlpath_resp_ch      nemo_m_out_controlpath_resp
    ,output t_nemo_controlpath_resp_valid   nemo_m_out_controlpath_resp_valid
    ,input  t_nemo_controlpath_resp_ready   nemo_m_in_controlpath_resp_ready
);

/*

BITS SO FAR:
18 = "system bit", 1=no translation, goes to pipeline_selector (addr 32 i think), 0=translation
    if translated, .addr <- pipeline_selector, .pipeline_local_sram_addr <- .addr
17 = pipeline config, can set filter range and map if 1
16 = combiner bit, 0/1 = datapath selector within pipeline (even/odd)
14-13: pipeline selector
15: goto translation unit
if [18:17] == 0 && [15] == 0, it will go to the pipeline, where [12:0] are used to select sram states
still have all bits 63:19 to use for virtualized controlpath
*/

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

t_nemo_controlpath_resp_ch      [N_OUTPUTS-1:0] nemo_s_in_controlpath_resp_postreg;
t_nemo_controlpath_resp_valid   [N_OUTPUTS-1:0] nemo_s_in_controlpath_resp_postreg_valid;
t_nemo_controlpath_resp_ready   [N_OUTPUTS-1:0] nemo_s_out_controlpath_resp_postreg_ready;
generate;
for (genvar output_i = 0; output_i < N_OUTPUTS; output_i++) begin
    nemo_controlpath_resp_fifo controlpath_resp_fifo_inst(
     .clk(clk)
    ,.rst(rst)

    ,.nemo_s_in_controlpath_resp    (nemo_s_in_controlpath_resp[output_i])
    ,.nemo_s_in_controlpath_resp_valid  (nemo_s_in_controlpath_resp_valid[output_i])
    ,.nemo_s_out_controlpath_resp_ready (nemo_s_out_controlpath_resp_ready[output_i])

    ,.nemo_m_out_controlpath_resp   (nemo_s_in_controlpath_resp_postreg[output_i])
    ,.nemo_m_out_controlpath_resp_valid (nemo_s_in_controlpath_resp_postreg_valid[output_i])
    ,.nemo_m_in_controlpath_resp_ready  (nemo_s_out_controlpath_resp_postreg_ready[output_i])
);
end
endgenerate

// the actual logic
// first, the REQ side.
logic outstanding_req;
logic [NUM_BITS-1:0] outstanding_req_selector;
logic [NUM_BITS-1:0] dest_selector;
always_ff @(posedge clk) begin
    if (rst) begin
        outstanding_req <= 0;
    end else begin
        if (outstanding_req) begin
            if (nemo_m_out_controlpath_resp_valid && nemo_m_in_controlpath_resp_ready) begin
                outstanding_req <= 0;
            end
        end else begin
            if (nemo_s_in_controlpath_req_postreg_valid && nemo_s_out_controlpath_req_postreg_ready) begin
                outstanding_req <= 1;
                outstanding_req_selector <= dest_selector;
            end
        end
    end
end

logic dest_ready;

assign dest_selector = nemo_s_in_controlpath_req_postreg.addr[END_BIT:START_BIT];
assign dest_ready = nemo_m_in_controlpath_req_ready[dest_selector];
assign nemo_s_out_controlpath_req_postreg_ready = !outstanding_req && dest_ready;
generate;
for (genvar output_i = 0; output_i < N_OUTPUTS; output_i++) begin
    assign nemo_m_out_controlpath_req[output_i] = nemo_s_in_controlpath_req_postreg;
    assign nemo_m_out_controlpath_req_valid[output_i] = nemo_s_in_controlpath_req_postreg_valid && (dest_selector == output_i) && !outstanding_req;
end
endgenerate

// Response path
assign nemo_m_out_controlpath_resp_valid = outstanding_req && nemo_s_in_controlpath_resp_postreg_valid[outstanding_req_selector];
generate;
for (genvar output_i = 0; output_i < N_OUTPUTS; output_i++) begin
    assign nemo_s_out_controlpath_resp_postreg_ready[output_i] = outstanding_req && nemo_m_in_controlpath_resp_ready && (outstanding_req_selector == output_i);
end
endgenerate
assign nemo_m_out_controlpath_resp = nemo_s_in_controlpath_resp_postreg[outstanding_req_selector];

endmodule
