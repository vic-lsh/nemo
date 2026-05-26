import nemo_defines::*;

module nemo_datapath_fifo #(
    parameter LOG_DEPTH = 2
) (
     input clk
    ,input rst

    ,input  t_nemo_datapath_ch      nemo_s_in_datapath
    ,input  t_nemo_datapath_valid   nemo_s_in_datapath_valid
    ,output t_nemo_datapath_ready   nemo_s_out_datapath_ready

    ,output t_nemo_datapath_ch      nemo_m_out_datapath
    ,output t_nemo_datapath_valid   nemo_m_out_datapath_valid
    ,input  t_nemo_datapath_ready   nemo_m_in_datapath_ready
);

nemo_fifo_valrdy #(
     .LOG_DEPTH     (LOG_DEPTH)
    ,.WIDTH         (NEMO_DATAPATH_WIDTH)
    ,.USE_LUTRAM    (1)
) nemo_datapath_fifo (
     .clk(clk)
    ,.rst(rst)
    
    ,.wrvalid(nemo_s_in_datapath_valid)
    ,.wrdata(nemo_s_in_datapath)
    ,.wrready(nemo_s_out_datapath_ready)
    
    ,.rdready(nemo_m_in_datapath_ready)
    ,.rddata(nemo_m_out_datapath)
    ,.rdvalid(nemo_m_out_datapath_valid)
    
    ,.approx_length()
);

endmodule
