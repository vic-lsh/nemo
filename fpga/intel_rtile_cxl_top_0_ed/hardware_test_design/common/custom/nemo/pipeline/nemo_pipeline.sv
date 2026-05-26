import afu_axi_if_pkg::*;
import nemo_defines::*;

module nemo_pipeline #(
     parameter N_DATAPATHS = 2
    ,parameter TRACKING_MODE = 0
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

t_nemo_controlpath_req_ch       [1:0]   demux_s_in_controlpath_req;
t_nemo_controlpath_req_valid    [1:0]   demux_s_in_controlpath_req_valid;
t_nemo_controlpath_req_ready    [1:0]   demux_s_out_controlpath_req_ready;

t_nemo_controlpath_resp_ch      [1:0]   demux_m_out_controlpath_resp;
t_nemo_controlpath_resp_valid   [1:0]   demux_m_out_controlpath_resp_valid;
t_nemo_controlpath_resp_ready   [1:0]   demux_m_in_controlpath_resp_ready;

nemo_controlpath_demux #(
     .END_BIT(17)
    ,.START_BIT(17)
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


logic [AFU_AXI_MAX_ADDR_WIDTH-1:0] added_stall;
logic [AFU_AXI_MAX_ADDR_WIDTH-1:0] filter_start;
logic [AFU_AXI_MAX_ADDR_WIDTH-1:0] filter_end;
logic [AFU_AXI_MAX_ADDR_WIDTH-1:0] map_norm_shift;

always @(posedge clk) begin
    if (rst) begin
        added_stall <= 0;
        filter_start <= 0;
        filter_end <= 0;
        map_norm_shift <= 0;
    end else begin
        if (demux_s_in_controlpath_req_valid[1] && demux_s_out_controlpath_req_ready[1] && demux_s_in_controlpath_req[1].req_metadata.is_write) begin
            if (demux_s_in_controlpath_req[1].addr[4:3] == 2'b00) begin
                added_stall <= demux_s_in_controlpath_req[1].write_data;
            end else if (demux_s_in_controlpath_req[1].addr[4:3] == 2'b01) begin
                filter_start <= demux_s_in_controlpath_req[1].write_data;
            end else if (demux_s_in_controlpath_req[1].addr[4:3] == 2'b10) begin
                filter_end <= demux_s_in_controlpath_req[1].write_data;
            end else if (demux_s_in_controlpath_req[1].addr[4:3] == 2'b11) begin
                map_norm_shift <= demux_s_in_controlpath_req[1].write_data;
            end
        end
    end
end

assign demux_s_out_controlpath_req_ready[1] = demux_m_in_controlpath_resp_ready[1];
assign demux_m_out_controlpath_resp_valid[1] = demux_s_in_controlpath_req_valid[1];
assign demux_m_out_controlpath_resp[1].read_data    = demux_s_in_controlpath_req[1].addr[4:3] == 2'b00 ? added_stall : demux_s_in_controlpath_req[1].addr[4:3] == 2'b01 ? filter_start : demux_s_in_controlpath_req[1].addr[4:3] == 2'b10 ? filter_end : map_norm_shift;
assign demux_m_out_controlpath_resp[1].req_metadata = demux_s_in_controlpath_req[1].req_metadata;
assign demux_m_out_controlpath_resp[1].resp_ok_n[0] = 0;//|demux_s_in_controlpath_req[1].addr[16:5];
assign demux_m_out_controlpath_resp[1].resp_ok_n[1] = 0;
assign demux_m_out_controlpath_resp[1].resp_ok_n[2] = 0;


t_nemo_datapath_ch      [N_DATAPATHS-1:0]   nemo_m_out_datapath;
t_nemo_datapath_valid   [N_DATAPATHS-1:0]   nemo_m_out_datapath_valid;
t_nemo_datapath_ready   [N_DATAPATHS-1:0]   nemo_m_in_datapath_ready;

nemo_pipeline_filter_and_flatmap #(
     .N_DATAPATHS(N_DATAPATHS)
) filter_and_flatmap_inst (
     .clk(clk)
    ,.rst(rst)

    ,.nemo_s_in_datapath    (nemo_s_in_datapath)
    ,.nemo_s_in_datapath_valid  (nemo_s_in_datapath_valid)
    ,.nemo_s_out_datapath_ready (nemo_s_out_datapath_ready)

    ,.nemo_m_out_datapath   (nemo_m_out_datapath)
    ,.nemo_m_out_datapath_valid (nemo_m_out_datapath_valid)
    ,.nemo_m_in_datapath_ready  (nemo_m_in_datapath_ready)

    ,.filter_start  (filter_start)
    ,.filter_end (filter_end)
    ,.map_norm_shift    (map_norm_shift)
);

nemo_pipeline_combiner_and_translation #(
      .N_DATAPATHS(N_DATAPATHS)
     ,.TRACKING_MODE(TRACKING_MODE)
) combiner_and_translation_inst (
     .clk(clk)
    ,.rst(rst)

    ,.nemo_s_in_datapath    (nemo_m_out_datapath)
    ,.nemo_s_in_datapath_valid  (nemo_m_out_datapath_valid)
    ,.nemo_s_out_datapath_ready (nemo_m_in_datapath_ready)

    ,.nemo_s_in_controlpath_req (demux_s_in_controlpath_req[0])
    ,.nemo_s_in_controlpath_req_valid   (demux_s_in_controlpath_req_valid[0])
    ,.nemo_s_out_controlpath_req_ready  (demux_s_out_controlpath_req_ready[0])

    ,.nemo_m_out_controlpath_resp   (demux_m_out_controlpath_resp[0])
    ,.nemo_m_out_controlpath_resp_valid (demux_m_out_controlpath_resp_valid[0])
    ,.nemo_m_in_controlpath_resp_ready  (demux_m_in_controlpath_resp_ready[0])

    ,.added_stall   (added_stall)
);

endmodule
