import afu_axi_if_pkg::*;
import nemo_defines::*;
import cxlip_top_pkg::*;

module nemo_top #(
     parameter SIM = 0
) (
     input                                              clk
    ,input                                              rst

    ,input  t_nemo_controlpath_req_ch                   controlpath_mmio_req
    ,input  t_nemo_controlpath_req_valid                controlpath_mmio_req_valid
    ,output t_nemo_controlpath_req_ready                controlpath_mmio_req_ready

    ,output t_nemo_controlpath_resp_ch                  controlpath_mmio_resp
    ,output t_nemo_controlpath_resp_valid               controlpath_mmio_resp_valid
    ,input  t_nemo_controlpath_resp_ready               controlpath_mmio_resp_ready
    
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

    ,output [17:0] errors_out
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


t_axi4_rd_addr_ch       [MC_CHANNEL-1:0]    controlpath_axi_ar;
t_axi4_rd_addr_ready    [MC_CHANNEL-1:0]    controlpath_axi_arready;
t_axi4_rd_resp_ch       [MC_CHANNEL-1:0]    controlpath_axi_r;
t_axi4_rd_resp_ready    [MC_CHANNEL-1:0]    controlpath_axi_rready;

t_axi4_wr_addr_ch       [MC_CHANNEL-1:0]    controlpath_axi_aw;
t_axi4_wr_addr_ready    [MC_CHANNEL-1:0]    controlpath_axi_awready;
t_axi4_wr_data_ch       [MC_CHANNEL-1:0]    controlpath_axi_w;
t_axi4_wr_data_ready    [MC_CHANNEL-1:0]    controlpath_axi_wready;
t_axi4_wr_resp_ch       [MC_CHANNEL-1:0]    controlpath_axi_b;
t_axi4_wr_resp_ready    [MC_CHANNEL-1:0]    controlpath_axi_bready;

logic [AFU_AXI_MAX_ADDR_WIDTH-1:0]controlpath_start_physical_address;

generate
for (genvar channel = 0; channel < MC_CHANNEL; channel++) begin : gen_count_per_channel
    logic [8:0] dmux_errors_out;
    assign errors_out[8+9*channel : 0+9*channel] = dmux_errors_out;
    nemo_path_splitter #(
         .SIM(SIM)
    ) path_splitter (
         .clk                           (clk)
        ,.rst                           (rst)

        ,.s_in_axi_ar                   (cxlip2iafu_axi_ar[channel])
        ,.s_out_axi_arready             (iafu2cxlip_axi_arready[channel])
        ,.s_out_axi_r                   (iafu2cxlip_axi_r[channel])
        ,.s_in_axi_rready               (cxlip2iafu_axi_rready[channel])

        ,.s_in_axi_aw                   (cxlip2iafu_axi_aw[channel])
        ,.s_out_axi_awready             (iafu2cxlip_axi_awready[channel])
        ,.s_in_axi_w                    (cxlip2iafu_axi_w[channel])
        ,.s_out_axi_wready              (iafu2cxlip_axi_wready[channel])
        ,.s_out_axi_b                   (iafu2cxlip_axi_b[channel])
        ,.s_in_axi_bready               (cxlip2iafu_axi_bready[channel])


        ,.datapath_m_out_axi_ar         (datapath_axi_ar[channel])
        ,.datapath_m_in_axi_arready     (datapath_axi_arready[channel])
        ,.datapath_m_in_axi_r           (datapath_axi_r[channel])
        ,.datapath_m_out_axi_rready     (datapath_axi_rready[channel])

        ,.datapath_m_out_axi_aw         (datapath_axi_aw[channel])
        ,.datapath_m_in_axi_awready     (datapath_axi_awready[channel])
        ,.datapath_m_out_axi_w          (datapath_axi_w[channel])
        ,.datapath_m_in_axi_wready      (datapath_axi_wready[channel])
        ,.datapath_m_in_axi_b           (datapath_axi_b[channel])
        ,.datapath_m_out_axi_bready     (datapath_axi_bready[channel])


        ,.controlpath_m_out_axi_ar      (controlpath_axi_ar[channel])
        ,.controlpath_m_in_axi_arready  (controlpath_axi_arready[channel])
        ,.controlpath_m_in_axi_r        (controlpath_axi_r[channel])
        ,.controlpath_m_out_axi_rready  (controlpath_axi_rready[channel])

        ,.controlpath_m_out_axi_aw      (controlpath_axi_aw[channel])
        ,.controlpath_m_in_axi_awready  (controlpath_axi_awready[channel])
        ,.controlpath_m_out_axi_w       (controlpath_axi_w[channel])
        ,.controlpath_m_in_axi_wready   (controlpath_axi_wready[channel])
        ,.controlpath_m_in_axi_b        (controlpath_axi_b[channel])
        ,.controlpath_m_out_axi_bready  (controlpath_axi_bready[channel])

	    ,.controlpath_start_physical_address  (controlpath_start_physical_address)

        ,.errors_out(dmux_errors_out)
    );

    // hook up datapath output to MC
    always_comb begin
        iafu2mc_axi_ar[channel]         = datapath_axi_ar[channel];
        datapath_axi_arready[channel]   = mc2iafu_axi_arready[channel];
        datapath_axi_r[channel]         = mc2iafu_axi_r[channel];
        iafu2mc_axi_rready[channel]     = datapath_axi_rready[channel];
        iafu2mc_axi_aw[channel]         = datapath_axi_aw[channel];
        datapath_axi_awready[channel]   = mc2iafu_axi_awready[channel];
        iafu2mc_axi_w[channel]          = datapath_axi_w[channel];
        datapath_axi_wready[channel]    = mc2iafu_axi_wready[channel];
        datapath_axi_b[channel]         = mc2iafu_axi_b[channel];
        iafu2mc_axi_bready[channel]     = datapath_axi_bready[channel];
    end
end
endgenerate

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

    ,.datapath_m_in_axi_arready (datapath_axi_arready)
    ,.datapath_m_in_axi_awready (datapath_axi_awready)

    ,.nemo_m_out_datapath       (merged_datapath)
    ,.nemo_m_out_datapath_valid (merged_datapath_valid)
    ,.nemo_m_in_datapath_ready  (merged_datapath_ready)
);

t_nemo_controlpath_req_ch      merged_controlpath_cxl_req;
t_nemo_controlpath_req_valid   merged_controlpath_cxl_req_valid;
t_nemo_controlpath_req_ready   merged_controlpath_cxl_req_ready;
nemo_axi_to_merged_controlpath_req axi_to_controlpath (
     .clk                               (clk)
    ,.rst                               (rst)

    ,.controlpath_s_in_axi_ar           (controlpath_axi_ar)
    ,.controlpath_s_out_axi_arready     (controlpath_axi_arready)
    ,.controlpath_s_in_axi_aw           (controlpath_axi_aw)
    ,.controlpath_s_out_axi_awready     (controlpath_axi_awready)
    ,.controlpath_s_in_axi_w            (controlpath_axi_w)
    ,.controlpath_s_out_axi_wready      (controlpath_axi_wready)

    ,.nemo_m_out_controlpath_req        (merged_controlpath_cxl_req)
    ,.nemo_m_out_controlpath_req_valid  (merged_controlpath_cxl_req_valid)
    ,.nemo_m_in_controlpath_req_ready   (merged_controlpath_cxl_req_ready)
);

t_nemo_controlpath_resp_ch       merged_controlpath_cxl_resp;
t_nemo_controlpath_resp_valid    merged_controlpath_cxl_resp_valid;
t_nemo_controlpath_resp_ready    merged_controlpath_cxl_resp_ready;
nemo_controlpath_resp_to_axi controlpath_to_axi (
     .clk                               (clk)
    ,.rst                               (rst)

    ,.nemo_s_in_controlpath_resp        (merged_controlpath_cxl_resp)
    ,.nemo_s_in_controlpath_resp_valid  (merged_controlpath_cxl_resp_valid)
    ,.nemo_s_out_controlpath_resp_ready (merged_controlpath_cxl_resp_ready)

    ,.controlpath_m_out_axi_r           (controlpath_axi_r)
    ,.controlpath_m_in_axi_rready       (controlpath_axi_rready)
    ,.controlpath_m_out_axi_b           (controlpath_axi_b)
    ,.controlpath_m_in_axi_bready       (controlpath_axi_bready)
);

t_nemo_controlpath_req_ch      merged_controlpath_req;
t_nemo_controlpath_req_valid   merged_controlpath_req_valid;
t_nemo_controlpath_req_ready   merged_controlpath_req_ready;
nemo_controlpath_req_merge cxl_mmio_control_merger (
     .clk(clk)
    ,.rst(rst)

    ,.controlpath_reqs_s_in             ({controlpath_mmio_req, merged_controlpath_cxl_req})
    ,.controlpath_reqs_s_in_valid       ({controlpath_mmio_req_valid, merged_controlpath_cxl_req_valid})
    ,.controlpath_reqs_s_out_thenready  ({controlpath_mmio_req_ready, merged_controlpath_cxl_req_ready})

    ,.controlpath_req_m_out             (merged_controlpath_req)
    ,.controlpath_req_m_out_valid       (merged_controlpath_req_valid)
    ,.controlpath_req_m_in_ready        (merged_controlpath_req_ready)
);

t_nemo_controlpath_resp_ch       merged_controlpath_resp;
t_nemo_controlpath_resp_valid    merged_controlpath_resp_valid;
t_nemo_controlpath_resp_ready    merged_controlpath_resp_ready;
nemo_controlpath_resp_demerge controlpath_resp_demerger (
     .clk(clk)
    ,.rst(rst)

    ,.s_in_controlpath_resp             (merged_controlpath_resp)
    ,.s_in_controlpath_resp_valid       (merged_controlpath_resp_valid)
    ,.s_out_controlpath_resp_thenready  (merged_controlpath_resp_ready)

    ,.m_out_controlpath_cxl_resp        (merged_controlpath_cxl_resp)
    ,.m_out_controlpath_cxl_resp_valid  (merged_controlpath_cxl_resp_valid)
    ,.m_in_controlpath_cxl_resp_ready   (merged_controlpath_cxl_resp_ready)

    ,.m_out_controlpath_mmio_resp       (controlpath_mmio_resp)
    ,.m_out_controlpath_mmio_resp_valid (controlpath_mmio_resp_valid)
    ,.m_in_controlpath_mmio_resp_ready  (controlpath_mmio_resp_ready)
);

nemo_telemetry_top #(
    .N_DATAPATHS(N_DATAPATHS)
) telem_top (
     .clk   (clk)
    ,.rst   (rst)

    ,.nemo_s_in_datapath                (merged_datapath)
    ,.nemo_s_in_datapath_valid          (merged_datapath_valid)
    ,.nemo_s_out_datapath_ready         (merged_datapath_ready)

    ,.nemo_s_in_controlpath_req         (merged_controlpath_req)
    ,.nemo_s_in_controlpath_req_valid   (merged_controlpath_req_valid)
    ,.nemo_s_out_controlpath_req_ready  (merged_controlpath_req_ready)

    ,.nemo_m_out_controlpath_resp       (merged_controlpath_resp)
    ,.nemo_m_out_controlpath_resp_valid (merged_controlpath_resp_valid)
    ,.nemo_m_in_controlpath_resp_ready  (merged_controlpath_resp_ready)

    ,.controlpath_start_physical_address(controlpath_start_physical_address)
);

endmodule
