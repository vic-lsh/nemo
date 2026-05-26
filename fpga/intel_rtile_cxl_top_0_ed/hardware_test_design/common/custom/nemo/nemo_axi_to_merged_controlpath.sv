import afu_axi_if_pkg::*;
import nemo_defines::*;
import cxlip_top_pkg::*;

module nemo_axi_to_merged_controlpath_req #(
     localparam PER_CONTROLPATH_REQ_QUEUE_LOG_DEPTH = 3
    ,localparam MERGED_CONTROLPATH_QUEUE_LOG_DEPTH = 5
) (
     input                                              clk
    ,input                                              rst

    // Controlpath Inputs
    ,input  t_axi4_rd_addr_ch       [MC_CHANNEL-1:0]    controlpath_s_in_axi_ar
    ,output t_axi4_rd_addr_ready    [MC_CHANNEL-1:0]    controlpath_s_out_axi_arready
   
    ,input  t_axi4_wr_addr_ch       [MC_CHANNEL-1:0]    controlpath_s_in_axi_aw
    ,output t_axi4_wr_addr_ready    [MC_CHANNEL-1:0]    controlpath_s_out_axi_awready
    ,input  t_axi4_wr_data_ch       [MC_CHANNEL-1:0]    controlpath_s_in_axi_w
    ,output t_axi4_wr_data_ready    [MC_CHANNEL-1:0]    controlpath_s_out_axi_wready

    // Controlpath req output
    ,output t_nemo_controlpath_req_ch                   nemo_m_out_controlpath_req
    ,output t_nemo_controlpath_req_valid                nemo_m_out_controlpath_req_valid
    ,input  t_nemo_controlpath_req_ready                nemo_m_in_controlpath_req_ready
);

// convert each req in each channel to a controlpath type
localparam CONTROLPATH_CONVERTER_OUTPUTS = 1;
localparam ALL_CONTROLPATHS = CONTROLPATH_CONVERTER_OUTPUTS * MC_CHANNEL;
t_nemo_controlpath_req_ch      [ALL_CONTROLPATHS-1:0]    all_controlpaths;
t_nemo_controlpath_req_valid   [ALL_CONTROLPATHS-1:0]    all_controlpaths_valid;
t_nemo_controlpath_req_ready   [ALL_CONTROLPATHS-1:0]    all_controlpaths_ready;
generate
for (genvar channel = 0; channel < MC_CHANNEL; channel++) begin: per_chan
    t_nemo_controlpath_req_ch      [CONTROLPATH_CONVERTER_OUTPUTS-1:0]    controlpaths_converted;
    t_nemo_controlpath_req_valid   [CONTROLPATH_CONVERTER_OUTPUTS-1:0]    controlpaths_converted_valid;
    t_nemo_controlpath_req_ready   [CONTROLPATH_CONVERTER_OUTPUTS-1:0]    controlpaths_converted_ready;
    nemo_axi_to_controlpath_req #(
         .N_OUTPUTS(CONTROLPATH_CONVERTER_OUTPUTS)
    ) controlpath_converter (
         .clk(clk)
        ,.rst(rst)

        ,.controlpath_s_in_axi_ar(controlpath_s_in_axi_ar[channel])
        ,.controlpath_s_out_axi_arready(controlpath_s_out_axi_arready[channel])
        ,.controlpath_s_in_axi_aw(controlpath_s_in_axi_aw[channel])
        ,.controlpath_s_out_axi_awready(controlpath_s_out_axi_awready[channel])
        ,.controlpath_s_in_axi_w(controlpath_s_in_axi_w[channel])
        ,.controlpath_s_out_axi_wready(controlpath_s_out_axi_wready[channel])
        ,.nemo_m_out_controlpath_reqs(controlpaths_converted)
        ,.nemo_m_out_controlpath_reqs_valid(controlpaths_converted_valid)
        ,.nemo_m_in_controlpath_reqs_ready(controlpaths_converted_ready)
        ,.channel(channel[CHANNEL_W-1:0])
    );
    // Throw all the converted controlpaths into one array
    for (genvar converter_output = 0; converter_output < CONTROLPATH_CONVERTER_OUTPUTS; converter_output++) begin: per_converter_output
        assign all_controlpaths      [CONTROLPATH_CONVERTER_OUTPUTS*channel + converter_output]    = controlpaths_converted      [converter_output];
        assign all_controlpaths_valid[CONTROLPATH_CONVERTER_OUTPUTS*channel + converter_output]    = controlpaths_converted_valid[converter_output];

        assign controlpaths_converted_ready[converter_output]                                      = all_controlpaths_ready[CONTROLPATH_CONVERTER_OUTPUTS*channel + converter_output];
    end
end

t_nemo_controlpath_req_ch      [ALL_CONTROLPATHS-1:0]    queued_controlpaths;
t_nemo_controlpath_req_valid   [ALL_CONTROLPATHS-1:0]    queued_controlpaths_valid;
t_nemo_controlpath_req_ready   [ALL_CONTROLPATHS-1:0]    queued_controlpaths_ready;
// not long queue for each controlpath channel
for (genvar controlpath_i = 0; controlpath_i < ALL_CONTROLPATHS; controlpath_i++) begin: per_controlpath
    nemo_controlpath_req_fifo #(
         .LOG_DEPTH (PER_CONTROLPATH_REQ_QUEUE_LOG_DEPTH)
    ) controlpath_req_fifo (
         .clk                   (clk)
        ,.rst                   (rst)

        ,.nemo_s_in_controlpath_req         (all_controlpaths[controlpath_i])
        ,.nemo_s_in_controlpath_req_valid   (all_controlpaths_valid[controlpath_i])
        ,.nemo_s_out_controlpath_req_ready  (all_controlpaths_ready[controlpath_i])

        ,.nemo_m_out_controlpath_req        (queued_controlpaths[controlpath_i])
        ,.nemo_m_out_controlpath_req_valid  (queued_controlpaths_valid[controlpath_i])
        ,.nemo_m_in_controlpath_req_ready   (queued_controlpaths_ready[controlpath_i])
    );        
end
endgenerate

t_nemo_controlpath_req_ch      merged_controlpath;
t_nemo_controlpath_req_valid   merged_controlpath_valid;
t_nemo_controlpath_req_ready   merged_controlpath_ready;
// merge the 2 controlpath channels into one
nemo_controlpath_req_merge #(
     .CONTROLPATHS_IN  (ALL_CONTROLPATHS)
) controlpath_req_merger (
     .clk                               (clk)
    ,.rst                               (rst)

    ,.controlpath_reqs_s_in            (queued_controlpaths)
    ,.controlpath_reqs_s_in_valid      (queued_controlpaths_valid)
    ,.controlpath_reqs_s_out_thenready (queued_controlpaths_ready)
    ,.controlpath_req_m_out            (merged_controlpath)
    ,.controlpath_req_m_out_valid      (merged_controlpath_valid)
    ,.controlpath_req_m_in_ready       (merged_controlpath_ready)
);

// long queue for combined controlpaths out
nemo_controlpath_req_fifo #(
     .LOG_DEPTH (MERGED_CONTROLPATH_QUEUE_LOG_DEPTH)
) controlpath_req_fifo (
     .clk                          (clk)
    ,.rst                          (rst)

    ,.nemo_s_in_controlpath_req         (merged_controlpath)
    ,.nemo_s_in_controlpath_req_valid   (merged_controlpath_valid)
    ,.nemo_s_out_controlpath_req_ready  (merged_controlpath_ready)

    ,.nemo_m_out_controlpath_req        (nemo_m_out_controlpath_req)
    ,.nemo_m_out_controlpath_req_valid  (nemo_m_out_controlpath_req_valid)
    ,.nemo_m_in_controlpath_req_ready   (nemo_m_in_controlpath_req_ready)
);        

endmodule
