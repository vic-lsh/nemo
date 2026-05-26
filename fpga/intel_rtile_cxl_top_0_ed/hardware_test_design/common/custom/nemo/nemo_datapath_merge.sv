import nemo_defines::*;

module nemo_datapath_merge #(
    parameter DATAPATHS_IN = 4
) (
     input  clk
    ,input  rst

    ,input  t_nemo_datapath_ch      [DATAPATHS_IN-1:0]  datapaths_s_in
    ,input  t_nemo_datapath_valid   [DATAPATHS_IN-1:0]  datapaths_s_in_valid
    ,output t_nemo_datapath_ready   [DATAPATHS_IN-1:0]  datapaths_s_out_thenready

    ,output t_nemo_datapath_ch                          datapath_m_out
    ,output t_nemo_datapath_valid                       datapath_m_out_valid
    ,input  t_nemo_datapath_ready                       datapath_m_in_ready
);

generate
if (DATAPATHS_IN == 4) begin
    nemo_datapath_merge_4 merge_4way (
            .clk(clk)
        ,.rst(rst)

        ,.datapaths_s_in    (datapaths_s_in)
        ,.datapaths_s_in_valid  (datapaths_s_in_valid)
        ,.datapaths_s_out_thenready (datapaths_s_out_thenready)

        ,.datapath_m_out    (datapath_m_out)
        ,.datapath_m_out_valid  (datapath_m_out_valid)
        ,.datapath_m_in_ready   (datapath_m_in_ready)
    );
end else if (DATAPATHS_IN == 2) begin
    nemo_datapath_merge_2 merge_2way (
         .clk(clk)
        ,.rst(rst)

        ,.stream_i    (datapaths_s_in)
        ,.stream_valid_i  (datapaths_s_in_valid)
        ,.stream_thenready_o  (datapaths_s_out_thenready)

        ,.merged_o    (datapath_m_out)
        ,.merged_valid_o  (datapath_m_out_valid)
        ,.merged_ready_i  (datapath_m_in_ready)
    );
end else begin
    err_datapaths_must_be_2_or_4 pls();
end
endgenerate

endmodule
