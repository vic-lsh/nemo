import afu_axi_if_pkg::*;
import nemo_defines::*;

module nemo_pipeline_combiner #(
     parameter N_DATAPATHS = 2
    ,parameter TRACKING_MODE = 0
) (
     input                                              clk
    ,input                                              rst

    ,input  t_nemo_datapath_ch      [N_DATAPATHS-1:0]   nemo_s_in_datapath
    ,input  t_nemo_datapath_valid   [N_DATAPATHS-1:0]   nemo_s_in_datapath_valid
    ,output t_nemo_datapath_ready   [N_DATAPATHS-1:0]   nemo_s_out_datapath_ready

    ,input  t_nemo_controlpath_req_ch                   nemo_s_in_controlpath_req
    ,input  t_nemo_controlpath_req_valid                nemo_s_in_controlpath_req_valid
    ,output t_nemo_controlpath_req_ready                nemo_s_out_controlpath_req_ready

    ,output t_nemo_controlpath_resp_ch                  nemo_m_out_controlpath_resp
    ,output t_nemo_controlpath_resp_valid               nemo_m_out_controlpath_resp_valid
    ,input  t_nemo_controlpath_resp_ready               nemo_m_in_controlpath_resp_ready

    ,input  logic [63:0]                                added_stall
);

generate
if (N_DATAPATHS == 1) begin
    nemo_pipeline_rmw #(
            .TRACKING_MODE(TRACKING_MODE)
        ) rmw_inst (
         .clk(clk)
        ,.rst(rst)

        ,.nemo_s_in_datapath    (nemo_s_in_datapath[0])
        ,.nemo_s_in_datapath_valid  (nemo_s_in_datapath_valid[0])
        ,.nemo_s_out_datapath_ready (nemo_s_out_datapath_ready[0])

        ,.nemo_s_in_controlpath_req (nemo_s_in_controlpath_req)
        ,.nemo_s_in_controlpath_req_valid   (nemo_s_in_controlpath_req_valid)
        ,.nemo_s_out_controlpath_req_ready  (nemo_s_out_controlpath_req_ready)

        ,.nemo_m_out_controlpath_resp   (nemo_m_out_controlpath_resp)
        ,.nemo_m_out_controlpath_resp_valid (nemo_m_out_controlpath_resp_valid)
        ,.nemo_m_in_controlpath_resp_ready  (nemo_m_in_controlpath_resp_ready)

        ,.added_stall   (added_stall)
    );
end else if (N_DATAPATHS == 2) begin
    t_nemo_controlpath_req_ch       [1:0]   demux_s_in_controlpath_req;
    t_nemo_controlpath_req_valid    [1:0]   demux_s_in_controlpath_req_valid;
    t_nemo_controlpath_req_ready    [1:0]   demux_s_out_controlpath_req_ready;

    t_nemo_controlpath_resp_ch      [1:0]   demux_m_out_controlpath_resp;
    t_nemo_controlpath_resp_valid   [1:0]   demux_m_out_controlpath_resp_valid;
    t_nemo_controlpath_resp_ready   [1:0]   demux_m_in_controlpath_resp_ready;

    nemo_controlpath_demux #(
        .END_BIT(16)
        ,.START_BIT(16)
    ) even_odd_demux (
         .clk(clk)
        ,.rst(rst)

        ,.nemo_s_in_controlpath_req (nemo_s_in_controlpath_req)
        ,.nemo_s_in_controlpath_req_valid   (nemo_s_in_controlpath_req_valid)
        ,.nemo_s_out_controlpath_req_ready  (nemo_s_out_controlpath_req_ready)

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
    for (genvar datapath_i = 0; datapath_i < N_DATAPATHS; datapath_i++) begin: per_datapath
        nemo_pipeline_rmw #(
            .TRACKING_MODE(TRACKING_MODE)
        ) rmw_inst2 (
            .clk(clk)
            ,.rst(rst)

            ,.nemo_s_in_datapath    (nemo_s_in_datapath[datapath_i])
            ,.nemo_s_in_datapath_valid  (nemo_s_in_datapath_valid[datapath_i])
            ,.nemo_s_out_datapath_ready (nemo_s_out_datapath_ready[datapath_i])

            ,.nemo_s_in_controlpath_req (demux_s_in_controlpath_req[datapath_i])
            ,.nemo_s_in_controlpath_req_valid   (demux_s_in_controlpath_req_valid[datapath_i])
            ,.nemo_s_out_controlpath_req_ready  (demux_s_out_controlpath_req_ready[datapath_i])

            ,.nemo_m_out_controlpath_resp   (demux_m_out_controlpath_resp[datapath_i])
            ,.nemo_m_out_controlpath_resp_valid (demux_m_out_controlpath_resp_valid[datapath_i])
            ,.nemo_m_in_controlpath_resp_ready  (demux_m_in_controlpath_resp_ready[datapath_i])

            ,.added_stall   (added_stall)
        );
    end
end else begin
    err_pls_set_n_datapaths_to_1_or_2 pls();
end
endgenerate
endmodule
