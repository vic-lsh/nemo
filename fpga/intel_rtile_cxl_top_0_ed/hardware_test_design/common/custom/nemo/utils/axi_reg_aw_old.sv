import afu_axi_if_pkg::*;

module axi_reg_aw_old #(
    parameter LOG_DEPTH = 2
) (
     input clk
    ,input rst

    ,input  t_axi4_wr_addr_ch           s_in_axi_aw
    ,output t_axi4_wr_addr_ready        s_out_axi_awready

    ,output t_axi4_wr_addr_ch           m_out_axi_aw
    ,input  t_axi4_wr_addr_ready        m_in_axi_awready
);

localparam AW_WIDTH = AFU_AXI_WR_ADDR_CH_WIDTH;

t_axi4_wr_addr_ch fifo_output;
logic fifo_output_valid;

// wastes one bit of space storing the valid bit for convenience.
nemo_fifo_valrdy #(
     .LOG_DEPTH  (LOG_DEPTH)
    ,.WIDTH     (AW_WIDTH)
    ,.USE_LUTRAM(1)
) axi_reg_aw_fifo (
     .clk(clk)
    ,.rst(rst)
    
    ,.wrvalid(s_in_axi_aw.awvalid)
    ,.wrdata(s_in_axi_aw)
    ,.wrready(s_out_axi_awready)
    
    ,.rdready(m_in_axi_awready)
    ,.rddata(fifo_output)
    ,.rdvalid(fifo_output_valid)
    
    ,.approx_length()
);

always_comb begin
    m_out_axi_aw.awid       = fifo_output.awid;
    m_out_axi_aw.awaddr     = fifo_output.awaddr;
    m_out_axi_aw.awlen      = fifo_output.awlen;
    m_out_axi_aw.awsize     = fifo_output.awsize;
    m_out_axi_aw.awburst    = fifo_output.awburst;
    m_out_axi_aw.awprot     = fifo_output.awprot;
    m_out_axi_aw.awqos      = fifo_output.awqos;
    m_out_axi_aw.awcache    = fifo_output.awcache;
    m_out_axi_aw.awlock     = fifo_output.awlock;
    m_out_axi_aw.awregion   = fifo_output.awregion;
    m_out_axi_aw.awuser     = fifo_output.awuser;

    m_out_axi_aw.awvalid    = fifo_output_valid;
end

endmodule