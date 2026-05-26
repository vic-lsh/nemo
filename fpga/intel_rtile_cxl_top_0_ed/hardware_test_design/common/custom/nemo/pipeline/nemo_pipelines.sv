import afu_axi_if_pkg::*;
import nemo_defines::*;

module nemo_pipelines #(
     parameter N_DATAPATHS = 2,
     parameter N_PIPELINES = 2
) (
     input                                  clk
    ,input                                  rst

    ,input  t_nemo_datapath_ch      [N_DATAPATHS-1:0]   nemo_s_in_datapath
    ,input  t_nemo_datapath_valid   [N_DATAPATHS-1:0]   nemo_s_in_datapath_valid
    ,output t_nemo_datapath_ready   [N_DATAPATHS-1:0]   nemo_s_out_datapath_ready

    ,input  t_nemo_controlpath_req_ch       nemo_s_in_controlpath_req
    ,input  t_nemo_controlpath_req_valid    nemo_s_in_controlpath_req_valid
    ,output t_nemo_controlpath_req_ready    nemo_s_out_controlpath_req_ready

    ,output t_nemo_controlpath_resp_ch      nemo_m_out_controlpath_resp
    ,output t_nemo_controlpath_resp_valid   nemo_m_out_controlpath_resp_valid
    ,input  t_nemo_controlpath_resp_ready   nemo_m_in_controlpath_resp_ready
);

t_nemo_controlpath_req_ch       [N_PIPELINES-1:0]   demux_s_in_controlpath_req;
t_nemo_controlpath_req_valid    [N_PIPELINES-1:0]   demux_s_in_controlpath_req_valid;
t_nemo_controlpath_req_ready    [N_PIPELINES-1:0]   demux_s_out_controlpath_req_ready;

t_nemo_controlpath_resp_ch      [N_PIPELINES-1:0]   demux_m_out_controlpath_resp;
t_nemo_controlpath_resp_valid   [N_PIPELINES-1:0]   demux_m_out_controlpath_resp_valid;
t_nemo_controlpath_resp_ready   [N_PIPELINES-1:0]   demux_m_in_controlpath_resp_ready;

if (N_PIPELINES == 1) begin
    nemo_controlpath_req_fifo controlpath_req_fifo_inst (
        .clk(clk)
        ,.rst(rst)

        ,.nemo_s_in_controlpath_req (nemo_s_in_controlpath_req)
        ,.nemo_s_in_controlpath_req_valid   (nemo_s_in_controlpath_req_valid)
        ,.nemo_s_out_controlpath_req_ready  (nemo_s_out_controlpath_req_ready)

        ,.nemo_m_out_controlpath_req    (demux_s_in_controlpath_req[0])
        ,.nemo_m_out_controlpath_req_valid  (demux_s_in_controlpath_req_valid[0])
        ,.nemo_m_in_controlpath_req_ready   (demux_s_out_controlpath_req_ready[0])
    );
    nemo_controlpath_resp_fifo controlpath_resp_fifo_inst(
        .clk(clk)
        ,.rst(rst)

        ,.nemo_s_in_controlpath_resp    (demux_m_out_controlpath_resp[0])
        ,.nemo_s_in_controlpath_resp_valid  (demux_m_out_controlpath_resp_valid[0])
        ,.nemo_s_out_controlpath_resp_ready (demux_m_in_controlpath_resp_ready[0])

        ,.nemo_m_out_controlpath_resp   (nemo_m_out_controlpath_resp)
        ,.nemo_m_out_controlpath_resp_valid (nemo_m_out_controlpath_resp_valid)
        ,.nemo_m_in_controlpath_resp_ready  (nemo_m_in_controlpath_resp_ready)
    );
end else begin
    localparam N_BITS_NEEDED = $clog2(N_PIPELINES);
    if (N_BITS_NEEDED > 4) begin
        err_pls_pick_fewer_pipelines err_me();
    end
    nemo_controlpath_demux #(
        .END_BIT(13+N_BITS_NEEDED-1)
        ,.START_BIT(13)
    ) ctrl_demux (
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
end

logic [N_PIPELINES-1:0][N_DATAPATHS-1:0]datapaths_ready;

for (genvar i = 0; i < N_PIPELINES; i++) begin
    nemo_pipeline #(
         .N_DATAPATHS(N_DATAPATHS)
        ,.TRACKING_MODE(i)
    ) pipeline_inst_i (
        .clk   (clk)
        ,.rst   (rst)

        // every pipeline gets a copy of the same datapath
        ,.nemo_s_in_datapath    (nemo_s_in_datapath)
        ,.nemo_s_in_datapath_valid  (nemo_s_in_datapath_valid)
        ,.nemo_s_out_datapath_ready (datapaths_ready[i])

        ,.nemo_s_in_controlpath_req    (demux_s_in_controlpath_req[i])
        ,.nemo_s_in_controlpath_req_valid  (demux_s_in_controlpath_req_valid[i])
        ,.nemo_s_out_controlpath_req_ready (demux_s_out_controlpath_req_ready[i])

        ,.nemo_m_out_controlpath_resp  (demux_m_out_controlpath_resp[i])
        ,.nemo_m_out_controlpath_resp_valid    (demux_m_out_controlpath_resp_valid[i])
        ,.nemo_m_in_controlpath_resp_ready     (demux_m_in_controlpath_resp_ready[i])
    );
end
assign nemo_s_out_datapath_ready = datapaths_ready[0];

endmodule
