import nemo_defines::*;

module nemo_controlpath_req_fifo #(
    parameter LOG_DEPTH = 2
) (
     input clk
    ,input rst

    ,input  t_nemo_controlpath_req_ch      nemo_s_in_controlpath_req
    ,input  t_nemo_controlpath_req_valid   nemo_s_in_controlpath_req_valid
    ,output t_nemo_controlpath_req_ready   nemo_s_out_controlpath_req_ready

    ,output t_nemo_controlpath_req_ch      nemo_m_out_controlpath_req
    ,output t_nemo_controlpath_req_valid   nemo_m_out_controlpath_req_valid
    ,input  t_nemo_controlpath_req_ready   nemo_m_in_controlpath_req_ready
);

nemo_fifo_valrdy #(
     .LOG_DEPTH     (LOG_DEPTH)
    ,.WIDTH         (NEMO_CONTROLPATH_REQ_WIDTH)
    ,.USE_LUTRAM    (1)
) nemo_controlpath_req_fifo (
     .clk(clk)
    ,.rst(rst)
    
    ,.wrvalid(nemo_s_in_controlpath_req_valid)
    ,.wrdata(nemo_s_in_controlpath_req)
    ,.wrready(nemo_s_out_controlpath_req_ready)
    
    ,.rdready(nemo_m_in_controlpath_req_ready)
    ,.rddata(nemo_m_out_controlpath_req)
    ,.rdvalid(nemo_m_out_controlpath_req_valid)
    
    ,.approx_length()
);

endmodule
