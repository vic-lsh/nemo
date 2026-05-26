import afu_axi_if_pkg::*;
import nemo_defines::*;

module nemo_telemetry_top #(
    parameter N_DATAPATHS = 2,
    localparam N_OVERFLOWS = 4
) (
     input                                  clk
    ,input                                  rst

    ,input  t_nemo_datapath_ch      [N_DATAPATHS-1:0]   nemo_s_in_datapath
    ,input  t_nemo_datapath_valid   [N_DATAPATHS-1:0]   nemo_s_in_datapath_valid
    ,output t_nemo_datapath_ready   [N_DATAPATHS-1:0]   nemo_s_out_datapath_ready

    ,input  logic                   [N_OVERFLOWS-1:0]   nemo_m_in_datapath_overflows

    ,input  t_nemo_controlpath_req_ch                   nemo_s_in_controlpath_req
    ,input  t_nemo_controlpath_req_valid                nemo_s_in_controlpath_req_valid
    ,output t_nemo_controlpath_req_ready                nemo_s_out_controlpath_req_ready

    ,output t_nemo_controlpath_resp_ch                  nemo_m_out_controlpath_resp
    ,output t_nemo_controlpath_resp_valid               nemo_m_out_controlpath_resp_valid
    ,input  t_nemo_controlpath_resp_ready               nemo_m_in_controlpath_resp_ready

    ,output [63:0]                                      controlpath_start_physical_address
);

t_nemo_controlpath_req_ch       [1:0]   demux_s_in_controlpath_req;
t_nemo_controlpath_req_valid    [1:0]   demux_s_in_controlpath_req_valid;
t_nemo_controlpath_req_ready    [1:0]   demux_s_out_controlpath_req_ready;

t_nemo_controlpath_resp_ch      [1:0]   demux_m_out_controlpath_resp;
t_nemo_controlpath_resp_valid   [1:0]   demux_m_out_controlpath_resp_valid;
t_nemo_controlpath_resp_ready   [1:0]   demux_m_in_controlpath_resp_ready;

localparam SYSTEM_BIT = 18;

logic [63:3] pipeline_selector;

t_nemo_controlpath_req_ch    postcxltrans_s_in_controlpath_req;
t_nemo_controlpath_req_valid postcxltrans_s_in_controlpath_req_valid;
t_nemo_controlpath_req_ready postcxltrans_s_out_controlpath_req_ready;

nemo_cxl_controlpath_translator cxl_translator_inst (
     .nemo_s_in_controlpath_req         (nemo_s_in_controlpath_req)
    ,.nemo_s_in_controlpath_req_valid   (nemo_s_in_controlpath_req_valid)
    ,.nemo_s_out_controlpath_req_ready  (nemo_s_out_controlpath_req_ready)

    ,.nemo_m_out_controlpath_req        (postcxltrans_s_in_controlpath_req)
    ,.nemo_m_out_controlpath_req_valid  (postcxltrans_s_in_controlpath_req_valid)
    ,.nemo_m_in_controlpath_req_ready   (postcxltrans_s_out_controlpath_req_ready)

    ,.controlpath_start_physical_address(controlpath_start_physical_address)
);

nemo_controlpath_demux #(
     .END_BIT(SYSTEM_BIT)
    ,.START_BIT(SYSTEM_BIT)
) ctrl_demux (
     .clk(clk)
    ,.rst(rst)

    ,.nemo_s_in_controlpath_req (postcxltrans_s_in_controlpath_req)
    ,.nemo_s_in_controlpath_req_valid   (postcxltrans_s_in_controlpath_req_valid)
    ,.nemo_s_out_controlpath_req_ready  (postcxltrans_s_out_controlpath_req_ready)

    ,.nemo_m_out_controlpath_req    (demux_s_in_controlpath_req)
    ,.nemo_m_out_controlpath_req_valid  (demux_s_in_controlpath_req_valid)
    ,.nemo_m_in_controlpath_req_ready   (demux_s_out_controlpath_req_ready)

    ,.nemo_s_in_controlpath_resp    (demux_m_out_controlpath_resp)
    ,.nemo_s_in_controlpath_resp_valid  (demux_m_out_controlpath_resp_valid)
    ,.nemo_s_out_controlpath_resp_ready (demux_m_in_controlpath_resp_ready)

    ,.nemo_m_out_controlpath_resp   (nemo_m_out_controlpath_resp)
    ,.nemo_m_out_controlpath_resp_valid (nemo_m_out_controlpath_resp_valid)
    ,.nemo_m_in_controlpath_resp_ready  (nemo_m_in_controlpath_resp_ready)
);

t_nemo_controlpath_req_ch    posttrans_s_in_controlpath_req;
t_nemo_controlpath_req_valid posttrans_s_in_controlpath_req_valid;
t_nemo_controlpath_req_ready posttrans_s_out_controlpath_req_ready;

nemo_controlpath_translator #(
     .SYSTEM_BIT(SYSTEM_BIT)
) translator_inst (
     .nemo_s_in_controlpath_req (demux_s_in_controlpath_req[0])
    ,.nemo_s_in_controlpath_req_valid   (demux_s_in_controlpath_req_valid[0])
    ,.nemo_s_out_controlpath_req_ready  (demux_s_out_controlpath_req_ready[0])

    ,.nemo_m_out_controlpath_req    (posttrans_s_in_controlpath_req)
    ,.nemo_m_out_controlpath_req_valid  (posttrans_s_in_controlpath_req_valid)
    ,.nemo_m_in_controlpath_req_ready   (posttrans_s_out_controlpath_req_ready)

    ,.pipeline_selector (pipeline_selector)
);

nemo_pipelines #(
     .N_DATAPATHS(N_DATAPATHS)
    // ,.TRACKING_MODE(0)
    ,.N_PIPELINES(2)
) all_pipelines_inst (
     .clk   (clk)
    ,.rst   (rst)

    ,.nemo_s_in_datapath    (nemo_s_in_datapath)
    ,.nemo_s_in_datapath_valid  (nemo_s_in_datapath_valid)
    ,.nemo_s_out_datapath_ready (nemo_s_out_datapath_ready)

    ,.nemo_s_in_controlpath_req (posttrans_s_in_controlpath_req)
    ,.nemo_s_in_controlpath_req_valid   (posttrans_s_in_controlpath_req_valid)
    ,.nemo_s_out_controlpath_req_ready  (posttrans_s_out_controlpath_req_ready)

    ,.nemo_m_out_controlpath_resp   (demux_m_out_controlpath_resp[0])
    ,.nemo_m_out_controlpath_resp_valid (demux_m_out_controlpath_resp_valid[0])
    ,.nemo_m_in_controlpath_resp_ready  (demux_m_in_controlpath_resp_ready[0])
);

// CHANGES:
// add sram addr to ctrlpath req
// modify that if this demux == 0 on output
// change rmw to use ctrlpath sram addr
// route based on old address bits 10:3 or whatever you need
// sorta an address translation layer if addr[19] == 0

// give ability to grab overflow bits
logic [N_OVERFLOWS-1:0] overflows_sticky;
localparam HOST_REGS_CNT = 2;
logic [63:0] reg_init_values[HOST_REGS_CNT];
assign reg_init_values[0] = 0; // pipeline_selector
assign reg_init_values[1] = 64'hffffffffffffffff; // controlpath_start_physical_address
logic [63:0] regs_in[HOST_REGS_CNT];
generate;
for (genvar i = 0; i < N_OVERFLOWS; i++) begin
    always @(posedge clk) begin
        if (rst) begin
            overflows_sticky[i] <= 0;
        end else begin
            if (demux_s_in_controlpath_req_valid[1] && demux_s_out_controlpath_req_ready[1] && (demux_s_in_controlpath_req[1].addr[SYSTEM_BIT-1:3] == i)) begin
                overflows_sticky[i] <= 0;
            end else if (nemo_m_in_datapath_overflows[i]) begin
                overflows_sticky[i] <= 1;
            end
        end
    end
end
for (genvar i = 0; i < HOST_REGS_CNT; i++) begin
    always @(posedge clk) begin
        if (rst) begin
            regs_in[i] <= reg_init_values[i];
        end else begin
            if (demux_s_in_controlpath_req_valid[1] && demux_s_out_controlpath_req_ready[1] && demux_s_in_controlpath_req[1].req_metadata.is_write && (demux_s_in_controlpath_req[1].addr[SYSTEM_BIT-1:3] == (i+4))) begin
                regs_in[i] <= demux_s_in_controlpath_req[1].write_data;
            end
        end
    end
end
endgenerate

assign pipeline_selector = regs_in[0][63:3];
assign controlpath_start_physical_address = regs_in[1];

assign demux_s_out_controlpath_req_ready[1] = demux_m_in_controlpath_resp_ready[1];
assign demux_m_out_controlpath_resp_valid[1] = demux_s_in_controlpath_req_valid[1];
assign demux_m_out_controlpath_resp[1].req_metadata = demux_s_in_controlpath_req[1].req_metadata;
assign demux_m_out_controlpath_resp[1].resp_ok_n[0] = 0;
assign demux_m_out_controlpath_resp[1].resp_ok_n[1] = 0;
assign demux_m_out_controlpath_resp[1].resp_ok_n[2] = 0;

always_comb begin
    demux_m_out_controlpath_resp[1].read_data = 64'hdead_be3f_f334_1235;

    if (demux_s_in_controlpath_req[1].addr[17:3] < 4) begin
        demux_m_out_controlpath_resp[1].read_data = overflows_sticky[demux_s_in_controlpath_req[1].addr[4:3]];
    end else if (demux_s_in_controlpath_req[1].addr[17:3] == 4) begin
        demux_m_out_controlpath_resp[1].read_data = regs_in[0]; // pipeline_selector
    end else if (demux_s_in_controlpath_req[1].addr[17:3] == 5) begin
        demux_m_out_controlpath_resp[1].read_data = regs_in[1]; // controlpath_start_physical_address
    end
end

endmodule
