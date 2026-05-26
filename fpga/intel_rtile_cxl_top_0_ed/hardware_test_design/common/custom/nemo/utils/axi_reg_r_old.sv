import afu_axi_if_pkg::*;

module axi_reg_r_old #(
    parameter LOG_DEPTH = 2
) (
     input clk
    ,input rst

    ,input  t_axi4_rd_resp_ch           m_in_axi_r
    ,output t_axi4_rd_resp_ready        m_out_axi_rready

    ,output t_axi4_rd_resp_ch           s_out_axi_r
    ,input  t_axi4_rd_resp_ready        s_in_axi_rready
);

localparam R_WIDTH = AFU_AXI_RD_RESP_CH_WIDTH;

t_axi4_rd_resp_ch fifo_output;
logic fifo_output_valid;

// wastes one bit of space storing the valid bit for convenience.
nemo_fifo_valrdy #(
     .LOG_DEPTH  (LOG_DEPTH)
    ,.WIDTH     (R_WIDTH)
    ,.USE_LUTRAM(1)
) axi_reg_r_fifo (
     .clk(clk)
    ,.rst(rst)
    
    ,.wrvalid(m_in_axi_r.rvalid)
    ,.wrdata(m_in_axi_r)
    ,.wrready(m_out_axi_rready)
    
    ,.rdready(s_in_axi_rready)
    ,.rddata(fifo_output)
    ,.rdvalid(fifo_output_valid)
    
    ,.approx_length()
);

always_comb begin
    s_out_axi_r.rid     = fifo_output.rid;
    s_out_axi_r.rdata   = fifo_output.rdata;
    s_out_axi_r.rresp   = fifo_output.rresp;
    s_out_axi_r.rlast   = fifo_output.rlast;
    s_out_axi_r.ruser   = fifo_output.ruser;

    s_out_axi_r.rvalid  = fifo_output_valid;
end

endmodule