import afu_axi_if_pkg::*;

module axi_reg_b_old #(
    parameter LOG_DEPTH = 2
) (
     input clk
    ,input rst

    ,input  t_axi4_wr_resp_ch           m_in_axi_b
    ,output t_axi4_wr_resp_ready        m_out_axi_bready

    ,output t_axi4_wr_resp_ch           s_out_axi_b
    ,input  t_axi4_wr_resp_ready        s_in_axi_bready
);

localparam B_WIDTH = AFU_AXI_WR_RESP_CH_WIDTH;

t_axi4_wr_resp_ch fifo_output;
logic fifo_output_valid;

// wastes one bit of space storing the valid bit for convenience.
nemo_fifo_valrdy #(
     .LOG_DEPTH  (LOG_DEPTH)
    ,.WIDTH     (B_WIDTH)
    ,.USE_LUTRAM(1)
) axi_reg_b_fifo (
     .clk(clk)
    ,.rst(rst)
    
    ,.wrvalid(m_in_axi_b.bvalid)
    ,.wrdata(m_in_axi_b)
    ,.wrready(m_out_axi_bready)
    
    ,.rdready(s_in_axi_bready)
    ,.rddata(fifo_output)
    ,.rdvalid(fifo_output_valid)
    
    ,.approx_length()
);

always_comb begin
    s_out_axi_b.bid     = fifo_output.bid;
    s_out_axi_b.bresp   = fifo_output.bresp;
    s_out_axi_b.buser   = fifo_output.buser;

    s_out_axi_b.bvalid  = fifo_output_valid;
end

endmodule