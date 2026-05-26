import afu_axi_if_pkg::*;
import nemo_defines::*;

module nemo_axi_to_controlpath_req #(
     parameter N_OUTPUTS = 1 // drop AW and W channel
) (
     input                                          clk
    ,input                                          rst

    // Controlpath Inputs
    ,input  t_axi4_rd_addr_ch                       controlpath_s_in_axi_ar
    ,output t_axi4_rd_addr_ready                    controlpath_s_out_axi_arready
    ,input  t_axi4_wr_addr_ch                       controlpath_s_in_axi_aw
    ,output t_axi4_wr_addr_ready                    controlpath_s_out_axi_awready
    ,input  t_axi4_wr_data_ch                       controlpath_s_in_axi_w
    ,output t_axi4_wr_data_ready                    controlpath_s_out_axi_wready

    // Controlpath output
    ,output t_nemo_controlpath_req_ch      [N_OUTPUTS-1:0] nemo_m_out_controlpath_reqs
    ,output t_nemo_controlpath_req_valid   [N_OUTPUTS-1:0] nemo_m_out_controlpath_reqs_valid
    ,input  t_nemo_controlpath_req_ready   [N_OUTPUTS-1:0] nemo_m_in_controlpath_reqs_ready

    ,input  logic                          [CHANNEL_W-1:0] channel
);

nemo_axi_ar_to_controlpath_reqs ar_converter (
     .clk                               (clk)
    ,.rst                               (rst)

    ,.controlpath_s_in_axi_ar           (controlpath_s_in_axi_ar)
    ,.controlpath_s_out_axi_arready     (controlpath_s_out_axi_arready)
    ,.nemo_m_out_controlpath_req        (nemo_m_out_controlpath_reqs[0])
    ,.nemo_m_out_controlpath_req_valid  (nemo_m_out_controlpath_reqs_valid[0])
    ,.nemo_m_in_controlpath_req_ready   (nemo_m_in_controlpath_reqs_ready[0])
    ,.channel                           (channel)
);

// no entry for AW or W channel bc we don't care about it ATM
assign controlpath_s_out_axi_awready = 0;
assign controlpath_s_out_axi_wready = 0;

generate
if (N_OUTPUTS != 1) begin
     this_module_doesnt_exist_so_err_pls_ datapath_converter_cant_handle_channels_except_ar();
end
endgenerate

endmodule
