import afu_axi_if_pkg::*;
import nemo_defines::*;

module nemo_axi_to_datapath #(
     parameter N_OUTPUTS = 2 // drop W channel
) (
     input                                          clk
    ,input                                          rst

    // Datapath Inputs (we don't get outputs)
    ,input  t_axi4_rd_addr_ch                       datapath_s_in_axi_ar
    ,input  t_axi4_wr_addr_ch                       datapath_s_in_axi_aw
    ,input  t_axi4_wr_data_ch                       datapath_s_in_axi_w

    ,input  t_axi4_rd_addr_ready                    datapath_m_in_axi_arready
    ,input  t_axi4_wr_addr_ready                    datapath_m_in_axi_awready

    // Datapath output
    ,output t_nemo_datapath_ch      [N_OUTPUTS-1:0] nemo_m_out_datapaths
    ,output t_nemo_datapath_valid   [N_OUTPUTS-1:0] nemo_m_out_datapaths_valid

    ,input  logic                   [CHANNEL_W-1:0] channel
);

nemo_axi_ar_to_datapath ar_converter (
     .clk                       (clk)
    ,.rst                       (rst)

    ,.datapath_s_in_axi_ar      (datapath_s_in_axi_ar)
    ,.datapath_m_in_axi_arready (datapath_m_in_axi_arready)
    ,.nemo_m_out_datapath       (nemo_m_out_datapaths[0])
    ,.nemo_m_out_datapath_valid (nemo_m_out_datapaths_valid[0])
    ,.channel                   (channel)
);

nemo_axi_aw_to_datapath aw_converter (
     .clk                       (clk)
    ,.rst                       (rst)

    ,.datapath_s_in_axi_aw      (datapath_s_in_axi_aw)
    ,.datapath_m_in_axi_awready (datapath_m_in_axi_awready)
    ,.nemo_m_out_datapath       (nemo_m_out_datapaths[1])
    ,.nemo_m_out_datapath_valid (nemo_m_out_datapaths_valid[1])
    ,.channel                   (channel)
);

// no entry for W channel bc we don't care about it ATM
t_axi4_wr_data_ch   _dontcare;
assign _dontcare = datapath_s_in_axi_w;

generate
if (N_OUTPUTS != 2) begin
     this_module_doesnt_exist_so_err_pls_ datapath_converter_cant_handle_channels_except_ar_and_aw();
end
endgenerate

endmodule
