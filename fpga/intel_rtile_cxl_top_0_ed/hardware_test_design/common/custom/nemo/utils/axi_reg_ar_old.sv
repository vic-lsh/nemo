import afu_axi_if_pkg::*;

module axi_reg_ar_old #(
    parameter LOG_DEPTH = 2
) (
     input clk
    ,input rst

    ,input  t_axi4_rd_addr_ch           s_in_axi_ar
    ,output t_axi4_rd_addr_ready        s_out_axi_arready

    ,output t_axi4_rd_addr_ch           m_out_axi_ar
    ,input  t_axi4_rd_addr_ready        m_in_axi_arready
);

localparam AR_WIDTH = AFU_AXI_RD_ADDR_CH_WIDTH;

t_axi4_rd_addr_ch fifo_output;
logic fifo_output_valid;

// wastes one bit of space storing the valid bit for convenience.
nemo_fifo_valrdy #(
     .LOG_DEPTH  (LOG_DEPTH)
    ,.WIDTH     (AR_WIDTH)
    ,.USE_LUTRAM(1)
) axi_reg_ar_fifo (
     .clk(clk)
    ,.rst(rst)
    
    ,.wrvalid(s_in_axi_ar.arvalid)
    ,.wrdata(s_in_axi_ar)
    ,.wrready(s_out_axi_arready)
    
    ,.rdready(m_in_axi_arready)
    ,.rddata(fifo_output)
    ,.rdvalid(fifo_output_valid)
    
    ,.approx_length()
);

always_comb begin
    m_out_axi_ar.arid       = fifo_output.arid;
    m_out_axi_ar.araddr     = fifo_output.araddr;
    m_out_axi_ar.arlen      = fifo_output.arlen;
    m_out_axi_ar.arsize     = fifo_output.arsize;
    m_out_axi_ar.arburst    = fifo_output.arburst;
    m_out_axi_ar.arprot     = fifo_output.arprot;
    m_out_axi_ar.arqos      = fifo_output.arqos;
    m_out_axi_ar.arcache    = fifo_output.arcache;
    m_out_axi_ar.arlock     = fifo_output.arlock;
    m_out_axi_ar.arregion   = fifo_output.arregion;
    m_out_axi_ar.aruser     = fifo_output.aruser;

    m_out_axi_ar.arvalid    = fifo_output_valid;
end

endmodule