import nemo_defines::*;

module nemo_datapath_merge_4 #(
    localparam DATAPATHS_IN = 4
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
    if (DATAPATHS_IN != 4) begin
        this_module_doesnt_exist_so_err_ datapaths_must_be_4_rn_bc_im_lazy();
    end
endgenerate

/*
module looks like this,
where + is a 2way merger:

in0-\
     +4-q---\
in1-/        \
              +----> output
in2-\        /
     +5-q---/
in3-/
*/
// currently hardcoded bc lazy
// 4 and 5 are intermediate outputs
// bq and aq mean before queue and after queue.

t_nemo_datapath_ch      [5:4]   internalbq;
t_nemo_datapath_valid   [5:4]   internalbq_valid;
t_nemo_datapath_ready   [5:4]   internalbq_ready;

t_nemo_datapath_ch      [5:4]   internalaq;
t_nemo_datapath_valid   [5:4]   internalaq_valid;
t_nemo_datapath_ready   [5:4]   internalaq_ready;

nemo_datapath_merge_2 in0_in1_merger (
     .clk                   (clk)
    ,.rst                   (rst)

    ,.stream_i              (datapaths_s_in[1:0])
    ,.stream_valid_i        (datapaths_s_in_valid[1:0])
    ,.stream_thenready_o    (datapaths_s_out_thenready[1:0])

    ,.merged_o              (internalbq[4])
    ,.merged_valid_o        (internalbq_valid[4])
    ,.merged_ready_i        (internalbq_ready[4])
);

nemo_datapath_merge_2 in2_in3_merger (
     .clk                   (clk)
    ,.rst                   (rst)

    ,.stream_i              (datapaths_s_in[3:2])
    ,.stream_valid_i        (datapaths_s_in_valid[3:2])
    ,.stream_thenready_o    (datapaths_s_out_thenready[3:2])

    ,.merged_o              (internalbq[5])
    ,.merged_valid_o        (internalbq_valid[5])
    ,.merged_ready_i        (internalbq_ready[5])
);

nemo_datapath_merge_2 in4_in5_merger (
     .clk                   (clk)
    ,.rst                   (rst)

    ,.stream_i              (internalaq[5:4])
    ,.stream_valid_i        (internalaq_valid[5:4])
    ,.stream_thenready_o    (internalaq_ready[5:4])

    ,.merged_o              (datapath_m_out)
    ,.merged_valid_o        (datapath_m_out_valid)
    ,.merged_ready_i        (datapath_m_in_ready)
);

generate
for (genvar qi=4; qi<=5; qi++) begin: tree_fifos
    nemo_datapath_fifo internalq_i (
         .clk                   (clk)
        ,.rst                   (rst)

        ,.nemo_s_in_datapath         (internalbq[qi])
        ,.nemo_s_in_datapath_valid   (internalbq_valid[qi])
        ,.nemo_s_out_datapath_ready  (internalbq_ready[qi])

        ,.nemo_m_out_datapath        (internalaq[qi])
        ,.nemo_m_out_datapath_valid  (internalaq_valid[qi])
        ,.nemo_m_in_datapath_ready   (internalaq_ready[qi])
    );
end
endgenerate

endmodule
