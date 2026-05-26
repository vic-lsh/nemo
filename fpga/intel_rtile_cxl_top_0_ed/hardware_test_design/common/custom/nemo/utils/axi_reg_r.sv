import afu_axi_if_pkg::*;

module axi_reg_r #(
    parameter LOG_DEPTH = 2
) (
     input clk
    ,input rst

    ,input  t_axi4_rd_resp_ch           m_in_axi_r
    ,output t_axi4_rd_resp_ready        m_out_axi_rready

    ,output t_axi4_rd_resp_ch           s_out_axi_r
    ,input  t_axi4_rd_resp_ready        s_in_axi_rready
);

localparam R_WIDTH = 0
                        + AFU_AXI_MAX_ID_WIDTH
                        + AFU_AXI_MAX_DATA_WIDTH
                        + $bits(t_axi4_resp_encoding)
                        + 1
                        + $bits(t_axi4_ruser)
                        + 0;
/*
        logic [AFU_AXI_MAX_ID_WIDTH-1:0]        rid;
        logic [AFU_AXI_MAX_DATA_WIDTH-1:0]      rdata;
        t_axi4_resp_encoding                    rresp;
        logic                                   rlast;
        logic                                   rvalid;
        t_axi4_ruser                            ruser;
*/

logic [R_WIDTH-1:0] fifo_input;
logic [R_WIDTH-1:0] fifo_output;
logic fifo_output_valid;

assign fifo_input = {m_in_axi_r.rid, m_in_axi_r.rdata, m_in_axi_r.rresp, m_in_axi_r.rlast, m_in_axi_r.ruser};
assign {s_out_axi_r.rid, s_out_axi_r.rdata, s_out_axi_r.rresp, s_out_axi_r.rlast, s_out_axi_r.ruser} = fifo_output;
assign s_out_axi_r.rvalid = fifo_output_valid;

// wastes one bit of space storing the valid bit for convenience.
nemo_fifo_valrdy #(
     .LOG_DEPTH  (LOG_DEPTH)
    ,.WIDTH     (R_WIDTH)
    ,.USE_LUTRAM(1)
) axi_reg_r_fifo (
     .clk(clk)
    ,.rst(rst)
    
    ,.wrvalid(m_in_axi_r.rvalid)
    ,.wrdata(fifo_input)
    ,.wrready(m_out_axi_rready)
    
    ,.rdready(s_in_axi_rready)
    ,.rddata(fifo_output)
    ,.rdvalid(fifo_output_valid)
    
    ,.approx_length()
);
generate;
    if (R_WIDTH + 1 != AFU_AXI_RD_RESP_CH_WIDTH) err_me_pls you_probs_got_axi_reg_r_wrong();
endgenerate

endmodule