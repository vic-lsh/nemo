import afu_axi_if_pkg::*;
import mc_axi_if_pkg::*;

module nemo_nemo_to_mc_axi (
    output t_to_mc_axi4          iafu2mc_to_mc_axi4_out,
    input  t_from_mc_axi4        mc2iafu_from_mc_axi4_in,

    input  t_axi4_rd_addr_ch     iafu2mc_axi_ar,
    output t_axi4_rd_addr_ready  mc2iafu_axi_arready,
    output t_axi4_rd_resp_ch     mc2iafu_axi_r,
    input  t_axi4_rd_resp_ready  iafu2mc_axi_rready,
   
    input  t_axi4_wr_addr_ch     iafu2mc_axi_aw,
    output t_axi4_wr_addr_ready  mc2iafu_axi_awready,
    input  t_axi4_wr_data_ch     iafu2mc_axi_w,
    output t_axi4_wr_data_ready  mc2iafu_axi_wready,
    output t_axi4_wr_resp_ch     mc2iafu_axi_b,
    input  t_axi4_wr_resp_ready  iafu2mc_axi_bready
);

always_comb begin
    // Read Address Channel (AR)
    iafu2mc_to_mc_axi4_out.arid      = iafu2mc_axi_ar.arid[MC_AXI_RAC_ID_BW-1:0];
    iafu2mc_to_mc_axi4_out.araddr    = iafu2mc_axi_ar.araddr[MC_AXI_RAC_ADDR_BW-1:0];
    iafu2mc_to_mc_axi4_out.arlen     = iafu2mc_axi_ar.arlen;
    iafu2mc_to_mc_axi4_out.arsize    = iafu2mc_axi_ar.arsize;
    iafu2mc_to_mc_axi4_out.arburst   = iafu2mc_axi_ar.arburst;
    iafu2mc_to_mc_axi4_out.arprot    = iafu2mc_axi_ar.arprot;
    iafu2mc_to_mc_axi4_out.arqos     = iafu2mc_axi_ar.arqos;
    iafu2mc_to_mc_axi4_out.arvalid   = iafu2mc_axi_ar.arvalid;
    iafu2mc_to_mc_axi4_out.arcache   = iafu2mc_axi_ar.arcache;
    iafu2mc_to_mc_axi4_out.arlock    = iafu2mc_axi_ar.arlock;
    iafu2mc_to_mc_axi4_out.arregion  = iafu2mc_axi_ar.arregion;
    iafu2mc_to_mc_axi4_out.aruser[0] = iafu2mc_axi_ar.aruser[0];

    mc2iafu_axi_arready = mc2iafu_from_mc_axi4_in.arready;

    // Read Data Channel (R)
    mc2iafu_axi_r.rid[MC_AXI_RRC_ID_BW-1:0]                    = mc2iafu_from_mc_axi4_in.rid;
    mc2iafu_axi_r.rid[AFU_AXI_MAX_ID_WIDTH-1:MC_AXI_RRC_ID_BW] = '0;
    mc2iafu_axi_r.rdata                                        = mc2iafu_from_mc_axi4_in.rdata;
    mc2iafu_axi_r.rresp                                        = mc2iafu_from_mc_axi4_in.rresp;
    mc2iafu_axi_r.rlast                                        = mc2iafu_from_mc_axi4_in.rlast;
    mc2iafu_axi_r.rvalid                                       = mc2iafu_from_mc_axi4_in.rvalid;
    mc2iafu_axi_r.ruser                                        = mc2iafu_from_mc_axi4_in.ruser;

    iafu2mc_to_mc_axi4_out.rready = iafu2mc_axi_rready;

    // Write Address Channel (AW)
    iafu2mc_to_mc_axi4_out.awid      = iafu2mc_axi_aw.awid[MC_AXI_WAC_ID_BW-1:0];
    iafu2mc_to_mc_axi4_out.awaddr    = iafu2mc_axi_aw.awaddr[MC_AXI_WAC_ADDR_BW-1:0];
    iafu2mc_to_mc_axi4_out.awlen     = iafu2mc_axi_aw.awlen;
    iafu2mc_to_mc_axi4_out.awsize    = iafu2mc_axi_aw.awsize;
    iafu2mc_to_mc_axi4_out.awburst   = iafu2mc_axi_aw.awburst;
    iafu2mc_to_mc_axi4_out.awprot    = iafu2mc_axi_aw.awprot;
    iafu2mc_to_mc_axi4_out.awqos     = iafu2mc_axi_aw.awqos;
    iafu2mc_to_mc_axi4_out.awvalid   = iafu2mc_axi_aw.awvalid;
    iafu2mc_to_mc_axi4_out.awcache   = iafu2mc_axi_aw.awcache;
    iafu2mc_to_mc_axi4_out.awlock    = iafu2mc_axi_aw.awlock;
    iafu2mc_to_mc_axi4_out.awregion  = iafu2mc_axi_aw.awregion;
    iafu2mc_to_mc_axi4_out.awuser[0] = iafu2mc_axi_aw.awuser[0];

    mc2iafu_axi_awready = mc2iafu_from_mc_axi4_in.awready;

    // Write Data Channel (W)
    iafu2mc_to_mc_axi4_out.wdata  = iafu2mc_axi_w.wdata;
    iafu2mc_to_mc_axi4_out.wstrb  = iafu2mc_axi_w.wstrb;
    iafu2mc_to_mc_axi4_out.wlast  = iafu2mc_axi_w.wlast;
    iafu2mc_to_mc_axi4_out.wvalid = iafu2mc_axi_w.wvalid;
    iafu2mc_to_mc_axi4_out.wuser  = iafu2mc_axi_w.wuser;

    mc2iafu_axi_wready = mc2iafu_from_mc_axi4_in.wready;

    // Write Response Channel (B)
    mc2iafu_axi_b.bid[MC_AXI_WRC_ID_BW-1:0]                    = mc2iafu_from_mc_axi4_in.bid;
    mc2iafu_axi_b.bid[AFU_AXI_MAX_ID_WIDTH-1:MC_AXI_WRC_ID_BW] = '0;
    mc2iafu_axi_b.bresp                                        = mc2iafu_from_mc_axi4_in.bresp;
    mc2iafu_axi_b.bvalid                                       = mc2iafu_from_mc_axi4_in.bvalid;
    mc2iafu_axi_b.buser[0]                                     = mc2iafu_from_mc_axi4_in.buser[0];
    mc2iafu_axi_b.buser[AFU_AXI_BUSER_WIDTH-1:1]               ='0;

    iafu2mc_to_mc_axi4_out.bready = iafu2mc_axi_bready;
end

endmodule
