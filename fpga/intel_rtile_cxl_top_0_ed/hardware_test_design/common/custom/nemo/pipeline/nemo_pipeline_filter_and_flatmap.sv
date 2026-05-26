import afu_axi_if_pkg::*;
import nemo_defines::*;

module nemo_pipeline_filter_and_flatmap #(
     parameter N_DATAPATHS = 2
) (
     input                                  clk
    ,input                                  rst

    ,input  t_nemo_datapath_ch      [N_DATAPATHS-1:0]   nemo_s_in_datapath
    ,input  t_nemo_datapath_valid   [N_DATAPATHS-1:0]   nemo_s_in_datapath_valid
    ,output t_nemo_datapath_ready   [N_DATAPATHS-1:0]   nemo_s_out_datapath_ready

    ,output t_nemo_datapath_ch      [N_DATAPATHS-1:0]   nemo_m_out_datapath
    ,output t_nemo_datapath_valid   [N_DATAPATHS-1:0]   nemo_m_out_datapath_valid
    ,input  t_nemo_datapath_ready   [N_DATAPATHS-1:0]   nemo_m_in_datapath_ready

    ,input  [AFU_AXI_MAX_ADDR_WIDTH-1:0]    filter_start
    ,input  [AFU_AXI_MAX_ADDR_WIDTH-1:0]    filter_end
    ,input  [AFU_AXI_MAX_ADDR_WIDTH-1:0]    map_norm_shift
);

generate
for (genvar datapath_i = 0; datapath_i < N_DATAPATHS; datapath_i++) begin
    t_nemo_datapath_ch              post_fifo_datapath;
    t_nemo_datapath_valid           post_fifo_datapath_valid;
    t_nemo_datapath_ready           post_fifo_datapath_ready;

    logic [AFU_AXI_MAX_ADDR_WIDTH-1:0] addr_post_subtract;
    logic [AFU_AXI_MAX_ADDR_WIDTH-1:0] addr_post_shift;
    logic filter_accept;

    always_comb begin
        // map part
        addr_post_subtract = post_fifo_datapath.addr - filter_start;
        addr_post_shift = addr_post_subtract >> map_norm_shift;
        filter_accept = post_fifo_datapath.addr >= filter_start && post_fifo_datapath.addr < filter_end;

        nemo_m_out_datapath[datapath_i] = post_fifo_datapath;
        nemo_m_out_datapath[datapath_i].pipeline_local_sram_addr = addr_post_shift[SRAM_LOG_DEPTH-1:0];
        if (map_norm_shift < SRAM_STATE_INTERNAL_W) begin
            nemo_m_out_datapath[datapath_i].sram_internal_addr = addr_post_subtract[SRAM_STATE_INTERNAL_W-1 : 0];
        end else begin
            nemo_m_out_datapath[datapath_i].sram_internal_addr = addr_post_subtract[(map_norm_shift-1) -: SRAM_STATE_INTERNAL_W];
        end

        // filter part
        nemo_m_out_datapath_valid[datapath_i] = post_fifo_datapath_valid && filter_accept;
        post_fifo_datapath_ready = nemo_m_in_datapath_ready[datapath_i] || !filter_accept;
    end

    nemo_datapath_fifo filter_fifo (
         .clk(clk)
        ,.rst(rst)

        ,.nemo_s_in_datapath    (nemo_s_in_datapath[datapath_i])
        ,.nemo_s_in_datapath_valid  (nemo_s_in_datapath_valid[datapath_i])
        ,.nemo_s_out_datapath_ready (nemo_s_out_datapath_ready[datapath_i])

        ,.nemo_m_out_datapath   (post_fifo_datapath)
        ,.nemo_m_out_datapath_valid (post_fifo_datapath_valid)
        ,.nemo_m_in_datapath_ready  (post_fifo_datapath_ready)
    );
end
endgenerate

endmodule
