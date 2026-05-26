import afu_axi_if_pkg::*;
import nemo_defines::*;
import cxlip_top_pkg::*;

module nemo_top_csrctrl #(
     parameter SIM = 0
) (
     input                                              clk
    ,input                                              rst

    ,input t_nemo_controlpath_req_ch                    controlpath_req
    ,input t_nemo_controlpath_req_valid                 controlpath_req_valid
    ,output t_nemo_controlpath_req_ready                controlpath_req_ready

    ,output t_nemo_controlpath_resp_ch                  controlpath_resp
    ,output t_nemo_controlpath_resp_valid               controlpath_resp_valid
    ,input t_nemo_controlpath_resp_ready                controlpath_resp_ready

    // AXI between CXL IP and this module
    ,input  t_axi4_rd_addr_ch       [MC_CHANNEL-1:0]    cxlip2iafu_axi_ar
    ,output t_axi4_rd_addr_ready    [MC_CHANNEL-1:0]    iafu2cxlip_axi_arready
    ,output t_axi4_rd_resp_ch       [MC_CHANNEL-1:0]    iafu2cxlip_axi_r
    ,input  t_axi4_rd_resp_ready    [MC_CHANNEL-1:0]    cxlip2iafu_axi_rready
   
    ,input  t_axi4_wr_addr_ch       [MC_CHANNEL-1:0]    cxlip2iafu_axi_aw
    ,output t_axi4_wr_addr_ready    [MC_CHANNEL-1:0]    iafu2cxlip_axi_awready
    ,input  t_axi4_wr_data_ch       [MC_CHANNEL-1:0]    cxlip2iafu_axi_w
    ,output t_axi4_wr_data_ready    [MC_CHANNEL-1:0]    iafu2cxlip_axi_wready
    ,output t_axi4_wr_resp_ch       [MC_CHANNEL-1:0]    iafu2cxlip_axi_b
    ,input  t_axi4_wr_resp_ready    [MC_CHANNEL-1:0]    cxlip2iafu_axi_bready

    // AXI between this module and MC (DRAM)
    ,output t_axi4_rd_addr_ch       [MC_CHANNEL-1:0]    iafu2mc_axi_ar
    ,input  t_axi4_rd_addr_ready    [MC_CHANNEL-1:0]    mc2iafu_axi_arready
    ,input  t_axi4_rd_resp_ch       [MC_CHANNEL-1:0]    mc2iafu_axi_r
    ,output t_axi4_rd_resp_ready    [MC_CHANNEL-1:0]    iafu2mc_axi_rready
   
    ,output t_axi4_wr_addr_ch       [MC_CHANNEL-1:0]    iafu2mc_axi_aw
    ,input  t_axi4_wr_addr_ready    [MC_CHANNEL-1:0]    mc2iafu_axi_awready
    ,output t_axi4_wr_data_ch       [MC_CHANNEL-1:0]    iafu2mc_axi_w
    ,input  t_axi4_wr_data_ready    [MC_CHANNEL-1:0]    mc2iafu_axi_wready
    ,input  t_axi4_wr_resp_ch       [MC_CHANNEL-1:0]    mc2iafu_axi_b
    ,output t_axi4_wr_resp_ready    [MC_CHANNEL-1:0]    iafu2mc_axi_bready

    // ,output [17:0] errors_out
);

/*
for aw, ar:
input -> isTelemReadWrite? -n> duplicate -> cxl
                                         -> telemUpdate
                           -y> telemControl (might need wdata here...)
for w, r, b:
input -> cxl


nemo_top:
(2x) nemo_path_splitter (in axi, out 2x axi)
(2x) (datapath axi out to mc)
nemo_axi_to_merged_datapath | nemo_axi_to_merged_controlpath
nemo_telemetry_top (in 1x control, 1x data)

nemo_path_splitter:
(2x) axi_reg
(2x) demux
(2x) datapath axi_reg | controlpath axi_reg

nemo_telemetry_top:
filter
flatmap
nemo_cpu

nemo_cpu: (STATE MACHINE)
if control path txn: read state, action 2 (e.g. cool), write back state, output state.
if data path txn: read state, action 1 (e.g. +1), write back state.

nemo_axi_to_merged_datapath (ar, aw, w):
drop(w)
(2x) nemo_axi_to_datapath = 3x nemo_datapath_type_converter
nemo_datapath_merger

nemo_controlpath_to_axi (r, b):
drop(b)
do the conversion & demerge

nemo_axi_to_controlpath (ar, aw, w):
(2x) ar_to_controlpath
nemo_controlpath_merger (now 2way)

*/

t_axi4_rd_addr_ch       [MC_CHANNEL-1:0]    datapath_axi_ar;
t_axi4_rd_addr_ready    [MC_CHANNEL-1:0]    datapath_axi_arready;
t_axi4_rd_resp_ch       [MC_CHANNEL-1:0]    datapath_axi_r;
t_axi4_rd_resp_ready    [MC_CHANNEL-1:0]    datapath_axi_rready;

t_axi4_wr_addr_ch       [MC_CHANNEL-1:0]    datapath_axi_aw;
t_axi4_wr_addr_ready    [MC_CHANNEL-1:0]    datapath_axi_awready;
t_axi4_wr_data_ch       [MC_CHANNEL-1:0]    datapath_axi_w;
t_axi4_wr_data_ready    [MC_CHANNEL-1:0]    datapath_axi_wready;
t_axi4_wr_resp_ch       [MC_CHANNEL-1:0]    datapath_axi_b;
t_axi4_wr_resp_ready    [MC_CHANNEL-1:0]    datapath_axi_bready;

generate
for (genvar channel = 0; channel < MC_CHANNEL; channel++) begin : gen_count_per_channel
    // hook up CXLIP to datapath
    assign datapath_axi_ar[channel]         = cxlip2iafu_axi_ar[channel];
    assign iafu2cxlip_axi_arready[channel]  = datapath_axi_arready[channel];
    assign iafu2cxlip_axi_r[channel]        = datapath_axi_r[channel];
    assign datapath_axi_rready[channel]     = cxlip2iafu_axi_rready[channel];
    assign datapath_axi_aw[channel]         = cxlip2iafu_axi_aw[channel];
    assign iafu2cxlip_axi_awready[channel]  = datapath_axi_awready[channel];
    assign datapath_axi_w[channel]          = cxlip2iafu_axi_w[channel];
    assign iafu2cxlip_axi_wready[channel]   = datapath_axi_wready[channel];
    assign iafu2cxlip_axi_b[channel]        = datapath_axi_b[channel];
    assign datapath_axi_bready[channel]     = cxlip2iafu_axi_bready[channel];

    // hook up datapath output to MC
    assign iafu2mc_axi_ar[channel]         = datapath_axi_ar[channel];
    assign datapath_axi_arready[channel]   = mc2iafu_axi_arready[channel];
    assign datapath_axi_r[channel]         = mc2iafu_axi_r[channel];
    assign iafu2mc_axi_rready[channel]     = datapath_axi_rready[channel];
    assign iafu2mc_axi_aw[channel]         = datapath_axi_aw[channel];
    assign datapath_axi_awready[channel]   = mc2iafu_axi_awready[channel];
    assign iafu2mc_axi_w[channel]          = datapath_axi_w[channel];
    assign datapath_axi_wready[channel]    = mc2iafu_axi_wready[channel];
    assign datapath_axi_b[channel]         = mc2iafu_axi_b[channel];
    assign iafu2mc_axi_bready[channel]     = datapath_axi_bready[channel];
end
endgenerate

logic [3:0] datapath_overflows;
localparam N_DATAPATHS = 2;
t_nemo_datapath_ch      [N_DATAPATHS-1:0] merged_datapath;
t_nemo_datapath_valid   [N_DATAPATHS-1:0] merged_datapath_valid;
t_nemo_datapath_ready   [N_DATAPATHS-1:0] merged_datapath_ready;
nemo_axi_to_merged_datapath #(
    .N_DATAPATHS_OUT(N_DATAPATHS)
) axi_to_datapath (
     .clk                       (clk)
    ,.rst                       (rst)

    ,.datapath_s_in_axi_ar      (datapath_axi_ar)
    ,.datapath_s_in_axi_aw      (datapath_axi_aw)
    ,.datapath_s_in_axi_w       (datapath_axi_w)

    ,.datapath_s_out_overflows  (datapath_overflows)

    ,.nemo_m_out_datapath       (merged_datapath)
    ,.nemo_m_out_datapath_valid (merged_datapath_valid)
    ,.nemo_m_in_datapath_ready  (merged_datapath_ready)
);

nemo_telemetry_top #(
    .N_DATAPATHS(N_DATAPATHS)
) telem_top (
     .clk   (clk)
    ,.rst   (rst)

    ,.nemo_s_in_datapath                (merged_datapath)
    ,.nemo_s_in_datapath_valid          (merged_datapath_valid)
    ,.nemo_s_out_datapath_ready         (merged_datapath_ready)

    ,.nemo_s_in_controlpath_req         (controlpath_req)
    ,.nemo_s_in_controlpath_req_valid   (controlpath_req_valid)
    ,.nemo_s_out_controlpath_req_ready  (controlpath_req_ready)

    ,.nemo_m_out_controlpath_resp       (controlpath_resp)
    ,.nemo_m_out_controlpath_resp_valid (controlpath_resp_valid)
    ,.nemo_m_in_controlpath_resp_ready  (controlpath_resp_ready)


    // ,.cxl_start_physical_address        (cxl_start_physical_address)
    // ,.telem_start_physical_address      (telem_start_physical_address)
);

endmodule
