import afu_axi_if_pkg::*;
import mc_axi_if_pkg::*;

module nemo_mc_axi_to_nemo (
     input  t_to_mc_axi4     cxlip2iafu_to_mc_axi4_in
    ,output t_from_mc_axi4   iafu2cxlip_from_mc_axi4_out

    ,output t_axi4_rd_addr_ch               cxlip2iafu_axi_ar
    ,input  t_axi4_rd_addr_ready            iafu2cxlip_axi_arready
    ,input  t_axi4_rd_resp_ch               iafu2cxlip_axi_r
    ,output t_axi4_rd_resp_ready            cxlip2iafu_axi_rready
   
    ,output t_axi4_wr_addr_ch               cxlip2iafu_axi_aw
    ,input  t_axi4_wr_addr_ready            iafu2cxlip_axi_awready
    ,output t_axi4_wr_data_ch               cxlip2iafu_axi_w
    ,input  t_axi4_wr_data_ready            iafu2cxlip_axi_wready
    ,input  t_axi4_wr_resp_ch               iafu2cxlip_axi_b
    ,output t_axi4_wr_resp_ready            cxlip2iafu_axi_bready
);

generate
// bit width checking
// left = nemo, right = input.
if (AFU_AXI_MAX_ID_WIDTH            <   MC_AXI_RAC_ID_BW    ) oh_no_ bad01(); // arid
if (AFU_AXI_MAX_ADDR_WIDTH          <   MC_AXI_RAC_ADDR_BW  ) oh_no_ bad02(); // araddr
if (AFU_AXI_MAX_BURST_LENGTH_WIDTH  <   MC_AXI_RAC_BLEN_BW  ) oh_no_ bad03(); // arlen
if (AFU_AXI_REGION_WIDTH            <   MC_AXI_RAC_REGION_BW) oh_no_ bad04(); // arregion
if (AFU_AXI_ARUSER_WIDTH            <   MC_AXI_RAC_USER_BW  ) oh_no_ bad05(); // aruser

if (AFU_AXI_MAX_ID_WIDTH            <   MC_AXI_RRC_ID_BW    ) oh_no_ bad06(); // rid
if (AFU_AXI_MAX_DATA_WIDTH          <   MC_AXI_RRC_DATA_BW  ) oh_no_ bad07(); // rdata
if (AFU_AXI_RUSER_WIDTH             <   MC_AXI_RRC_USER_BW  ) oh_no_ bad08(); // ruser

if (AFU_AXI_MAX_ID_WIDTH            <   MC_AXI_WAC_ID_BW    ) oh_no_ bad09(); // awid
if (AFU_AXI_MAX_ADDR_WIDTH          <   MC_AXI_WAC_ADDR_BW  ) oh_no_ bad10(); // awaddr
if (AFU_AXI_MAX_BURST_LENGTH_WIDTH  <   MC_AXI_WAC_BLEN_BW  ) oh_no_ bad11(); // awlen
if (AFU_AXI_REGION_WIDTH            <   MC_AXI_WAC_REGION_BW) oh_no_ bad12(); // awregion
if (AFU_AXI_AWUSER_WIDTH            <   MC_AXI_WAC_USER_BW  ) oh_no_ bad13(); // awuser

if (AFU_AXI_MAX_DATA_WIDTH          <   MC_AXI_WDC_DATA_BW  ) oh_no_ bad14(); // wdata
if (AFU_AXI_MAX_DATA_WIDTH/8        <   MC_AXI_WDC_STRB_BW  ) oh_no_ bad15(); // wstrb
if (AFU_AXI_WUSER_WIDTH             <   MC_AXI_WDC_USER_BW  ) oh_no_ bad16(); // wuser

if (AFU_AXI_MAX_ID_WIDTH            <   MC_AXI_WRC_ID_BW    ) oh_no_ bad17(); // bid
if (AFU_AXI_BUSER_WIDTH             <   MC_AXI_WRC_USER_BW  ) oh_no_ bad18(); // buser
endgenerate

always_comb begin
    cxlip2iafu_axi_ar.arid[MC_AXI_RAC_ID_BW-1:0] = cxlip2iafu_to_mc_axi4_in.arid;
    cxlip2iafu_axi_ar.arid[AFU_AXI_MAX_ID_WIDTH-1:MC_AXI_RAC_ID_BW] = '0;
    cxlip2iafu_axi_ar.araddr[MC_AXI_RAC_ADDR_BW-1:0]            = cxlip2iafu_to_mc_axi4_in.araddr;
    cxlip2iafu_axi_ar.araddr[AFU_AXI_MAX_ADDR_WIDTH-1:MC_AXI_RAC_ADDR_BW]            = '0;
    cxlip2iafu_axi_ar.arlen             = cxlip2iafu_to_mc_axi4_in.arlen;
    cxlip2iafu_axi_ar.arsize            = cxlip2iafu_to_mc_axi4_in.arsize;
    cxlip2iafu_axi_ar.arburst           = cxlip2iafu_to_mc_axi4_in.arburst;
    cxlip2iafu_axi_ar.arprot            = cxlip2iafu_to_mc_axi4_in.arprot;
    cxlip2iafu_axi_ar.arqos             = cxlip2iafu_to_mc_axi4_in.arqos;
    cxlip2iafu_axi_ar.arvalid           = cxlip2iafu_to_mc_axi4_in.arvalid;
    cxlip2iafu_axi_ar.arcache           = cxlip2iafu_to_mc_axi4_in.arcache;
    cxlip2iafu_axi_ar.arlock            = cxlip2iafu_to_mc_axi4_in.arlock;
    cxlip2iafu_axi_ar.arregion          = cxlip2iafu_to_mc_axi4_in.arregion;
    cxlip2iafu_axi_ar.aruser[0]            = cxlip2iafu_to_mc_axi4_in.aruser[0];
    cxlip2iafu_axi_ar.aruser[AFU_AXI_ARUSER_WIDTH-1:1]            = '0;

    iafu2cxlip_from_mc_axi4_out.arready = iafu2cxlip_axi_arready;

    iafu2cxlip_from_mc_axi4_out.rid     = iafu2cxlip_axi_r.rid[MC_AXI_RRC_ID_BW-1:0];
    iafu2cxlip_from_mc_axi4_out.rdata   = iafu2cxlip_axi_r.rdata;
    iafu2cxlip_from_mc_axi4_out.rresp   = iafu2cxlip_axi_r.rresp;
    iafu2cxlip_from_mc_axi4_out.rlast   = iafu2cxlip_axi_r.rlast;
    iafu2cxlip_from_mc_axi4_out.rvalid  = iafu2cxlip_axi_r.rvalid;
    iafu2cxlip_from_mc_axi4_out.ruser   = iafu2cxlip_axi_r.ruser;

    cxlip2iafu_axi_rready               = cxlip2iafu_to_mc_axi4_in.rready;

    cxlip2iafu_axi_aw.awid[MC_AXI_WAC_ID_BW-1:0] = cxlip2iafu_to_mc_axi4_in.awid;
    cxlip2iafu_axi_aw.awid[AFU_AXI_MAX_ID_WIDTH-1:MC_AXI_RAC_ID_BW] = '0;
    cxlip2iafu_axi_aw.awaddr[MC_AXI_WAC_ADDR_BW-1:0]            = cxlip2iafu_to_mc_axi4_in.awaddr;
    cxlip2iafu_axi_aw.awaddr[AFU_AXI_MAX_ADDR_WIDTH-1:MC_AXI_WAC_ADDR_BW]            = '0;
    cxlip2iafu_axi_aw.awlen             = cxlip2iafu_to_mc_axi4_in.awlen;
    cxlip2iafu_axi_aw.awsize            = cxlip2iafu_to_mc_axi4_in.awsize;
    cxlip2iafu_axi_aw.awburst           = cxlip2iafu_to_mc_axi4_in.awburst;
    cxlip2iafu_axi_aw.awprot            = cxlip2iafu_to_mc_axi4_in.awprot;
    cxlip2iafu_axi_aw.awqos             = cxlip2iafu_to_mc_axi4_in.awqos;
    cxlip2iafu_axi_aw.awvalid           = cxlip2iafu_to_mc_axi4_in.awvalid;
    cxlip2iafu_axi_aw.awcache           = cxlip2iafu_to_mc_axi4_in.awcache;
    cxlip2iafu_axi_aw.awlock            = cxlip2iafu_to_mc_axi4_in.awlock;
    cxlip2iafu_axi_aw.awregion          = cxlip2iafu_to_mc_axi4_in.awregion;
    cxlip2iafu_axi_aw.awuser[0]            = cxlip2iafu_to_mc_axi4_in.awuser[0];
    cxlip2iafu_axi_ar.aruser[AFU_AXI_AWUSER_WIDTH-1:1]            = '0;

    iafu2cxlip_from_mc_axi4_out.awready = iafu2cxlip_axi_awready;

    cxlip2iafu_axi_w.wdata              = cxlip2iafu_to_mc_axi4_in.wdata;
    cxlip2iafu_axi_w.wstrb              = cxlip2iafu_to_mc_axi4_in.wstrb;
    cxlip2iafu_axi_w.wlast              = cxlip2iafu_to_mc_axi4_in.wlast;
    cxlip2iafu_axi_w.wvalid             = cxlip2iafu_to_mc_axi4_in.wvalid;
    cxlip2iafu_axi_w.wuser              = cxlip2iafu_to_mc_axi4_in.wuser;

    iafu2cxlip_from_mc_axi4_out.wready  = iafu2cxlip_axi_wready;

    iafu2cxlip_from_mc_axi4_out.bid     = iafu2cxlip_axi_b.bid[MC_AXI_WRC_ID_BW];
    iafu2cxlip_from_mc_axi4_out.bresp   = iafu2cxlip_axi_b.bresp;
    iafu2cxlip_from_mc_axi4_out.bvalid  = iafu2cxlip_axi_b.bvalid;
    iafu2cxlip_from_mc_axi4_out.buser   = iafu2cxlip_axi_b.buser[0];

    cxlip2iafu_axi_bready               = cxlip2iafu_to_mc_axi4_in.bready;
end

endmodule