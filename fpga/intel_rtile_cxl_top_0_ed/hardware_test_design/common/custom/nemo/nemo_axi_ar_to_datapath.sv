import afu_axi_if_pkg::*;
import nemo_defines::*;

module nemo_axi_ar_to_datapath (
     input                              clk
    ,input                              rst

    // Datapath Inputs
    ,input  t_axi4_rd_addr_ch           datapath_s_in_axi_ar
    ,input  t_axi4_rd_addr_ready        datapath_m_in_axi_arready

    // Datapath output
    ,output t_nemo_datapath_ch          nemo_m_out_datapath
    ,output t_nemo_datapath_valid       nemo_m_out_datapath_valid

    ,input  logic   [CHANNEL_W-1:0]     channel
);
always_comb begin
     nemo_m_out_datapath_valid               = datapath_s_in_axi_ar.arvalid && datapath_m_in_axi_arready;
     nemo_m_out_datapath.src_channel         = channel;
     nemo_m_out_datapath.is_write            = 0;
     nemo_m_out_datapath.addr                = datapath_s_in_axi_ar.araddr;
     nemo_m_out_datapath.transaction_bytes   = (datapath_s_in_axi_ar.arlen + 1) << datapath_s_in_axi_ar.arsize;
end
endmodule
