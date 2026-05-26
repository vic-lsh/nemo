import nemo_defines::*;

module nemo_controlpath_resp_fifo #(
    parameter LOG_DEPTH = 2
) (
     input clk
    ,input rst

    ,input  t_nemo_controlpath_resp_ch      nemo_s_in_controlpath_resp
    ,input  t_nemo_controlpath_resp_valid   nemo_s_in_controlpath_resp_valid
    ,output t_nemo_controlpath_resp_ready   nemo_s_out_controlpath_resp_ready

    ,output t_nemo_controlpath_resp_ch      nemo_m_out_controlpath_resp
    ,output t_nemo_controlpath_resp_valid   nemo_m_out_controlpath_resp_valid
    ,input  t_nemo_controlpath_resp_ready   nemo_m_in_controlpath_resp_ready
);

nemo_fifo_valrdy #(
     .LOG_DEPTH     (LOG_DEPTH)
    ,.WIDTH         (NEMO_CONTROLPATH_RESP_WIDTH)
    ,.USE_LUTRAM    (1)
) nemo_controlpath_resp_fifo (
     .clk(clk)
    ,.rst(rst)
    
    ,.wrvalid(nemo_s_in_controlpath_resp_valid)
    ,.wrdata(nemo_s_in_controlpath_resp)
    ,.wrready(nemo_s_out_controlpath_resp_ready)
    
    ,.rdready(nemo_m_in_controlpath_resp_ready)
    ,.rddata(nemo_m_out_controlpath_resp)
    ,.rdvalid(nemo_m_out_controlpath_resp_valid)
    
    ,.approx_length()
);

endmodule
