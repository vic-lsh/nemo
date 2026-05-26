import afu_axi_if_pkg::*;
import nemo_defines::*;

module nemo_axi_aw_to_datapath (
     input                              clk
    ,input                              rst

    // Datapath Inputs
    ,input  t_axi4_wr_addr_ch           datapath_s_in_axi_aw
    ,input  t_axi4_wr_addr_ready        datapath_m_in_axi_awready

    // Datapath output
    ,output t_nemo_datapath_ch          nemo_m_out_datapath
    ,output t_nemo_datapath_valid       nemo_m_out_datapath_valid

    ,input  logic   [CHANNEL_W-1:0]     channel
);
always_comb begin
     nemo_m_out_datapath_valid               = datapath_s_in_axi_aw.awvalid && datapath_m_in_axi_awready;
     nemo_m_out_datapath.src_channel         = channel;
     nemo_m_out_datapath.is_write            = 1;
     nemo_m_out_datapath.addr                = datapath_s_in_axi_aw.awaddr;
     nemo_m_out_datapath.transaction_bytes   = (datapath_s_in_axi_aw.awlen + 1) << datapath_s_in_axi_aw.awsize;
end
endmodule
