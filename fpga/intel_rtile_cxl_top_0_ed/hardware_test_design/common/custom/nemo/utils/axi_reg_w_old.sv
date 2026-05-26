import afu_axi_if_pkg::*;

module axi_reg_w_old #(
    parameter LOG_DEPTH = 2
) (
     input clk
    ,input rst

    ,input  t_axi4_wr_data_ch           s_in_axi_w
    ,output t_axi4_wr_data_ready        s_out_axi_wready

    ,output t_axi4_wr_data_ch           m_out_axi_w
    ,input  t_axi4_wr_data_ready        m_in_axi_wready
);

localparam W_WIDTH = AFU_AXI_WR_DATA_CH_WIDTH;

t_axi4_wr_data_ch fifo_output;
logic fifo_output_valid;

// wastes one bit of space storing the valid bit for convenience.
nemo_fifo_valrdy #(
     .LOG_DEPTH  (LOG_DEPTH)
    ,.WIDTH     (W_WIDTH)
    ,.USE_LUTRAM(1)
) axi_reg_w_fifo (
     .clk(clk)
    ,.rst(rst)
    
    ,.wrvalid(s_in_axi_w.wvalid)
    ,.wrdata(s_in_axi_w)
    ,.wrready(s_out_axi_wready)
    
    ,.rdready(m_in_axi_wready)
    ,.rddata(fifo_output)
    ,.rdvalid(fifo_output_valid)
    
    ,.approx_length()
);

always_comb begin
    m_out_axi_w.wdata      = fifo_output.wdata;
    m_out_axi_w.wstrb      = fifo_output.wstrb;
    m_out_axi_w.wlast      = fifo_output.wlast;
    m_out_axi_w.wuser      = fifo_output.wuser;

    m_out_axi_w.wvalid    = fifo_output_valid;
end

endmodule