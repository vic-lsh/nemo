`timescale 1 ns / 1 ps

// import cxlip_top_pkg::*;

module fake_axi_dev #(
    parameter FIFO_LOGDEPTH = 9,
    parameter BACKPRESSURE = 0
) (

    input  logic                            clk,
    input  logic                            rst,

    input  mc_axi_if_pkg::t_to_mc_axi4      s_axi_in,
    output mc_axi_if_pkg::t_from_mc_axi4    s_axi_out,

    output logic [1:0]errors_sticky
);

logic ridf_wr_en;
logic ridf_full; // if true, ERROR
logic ridf_empty;

SoftFIFO #(
     .LOG_DEPTH  (FIFO_LOGDEPTH)
    ,.WIDTH     (mc_axi_if_pkg::MC_AXI_WRC_ID_BW)
) rid_fifo (
     .clock(clk)
    ,.reset_n(~rst)
    ,.wrreq(s_axi_in.arvalid && ridf_wr_en)
    ,.data(s_axi_in.arid)
    ,.full(ridf_full)
    ,.q(s_axi_out.rid)
    ,.empty(ridf_empty)
    ,.rdreq(s_axi_in.rready)
);
assign s_axi_out.rdata = '0;
assign s_axi_out.rresp = afu_axi_if_pkg::eresp_SLVERR;
assign s_axi_out.rvalid = !ridf_empty;
assign s_axi_out.rlast = 1'b1;
assign s_axi_out.ruser = mc_axi_if_pkg::t_rd_rsp_user'(1'b0); // poison

logic bidf_wrreq;
logic [mc_axi_if_pkg::MC_AXI_WRC_ID_BW-1:0] awid_q;


logic bidf_full; // if true, ERROR
logic bidf_empty;

SoftFIFO #(
     .LOG_DEPTH  (FIFO_LOGDEPTH)
    ,.WIDTH     (mc_axi_if_pkg::MC_AXI_WRC_ID_BW)
) bid_fifo (
     .clock(clk)
    ,.reset_n(~rst)
    ,.wrreq(bidf_wrreq)
    ,.data(awid_q)
    ,.full(bidf_full)
    ,.q(s_axi_out.bid)
    ,.empty(bidf_empty)
    ,.rdreq(s_axi_in.bready)
);
assign s_axi_out.bresp = afu_axi_if_pkg::eresp_SLVERR;
assign s_axi_out.bvalid = !bidf_empty;
assign s_axi_out.buser = mc_axi_if_pkg::t_rd_rsp_user'(1'b0); // must tie to 0

logic awidf_wr_en;
logic awidf_full; // if true, ERROR
logic awidf_empty;
logic awidf_rdreq;

SoftFIFO #(
     .LOG_DEPTH  (FIFO_LOGDEPTH)
    ,.WIDTH     (mc_axi_if_pkg::MC_AXI_WRC_ID_BW)
) awid_fifo (
     .clock(clk)
    ,.reset_n(~rst)
    ,.wrreq(s_axi_in.awvalid && awidf_wr_en)
    ,.data(s_axi_in.awid)
    ,.full(awidf_full)
    ,.q(awid_q)
    ,.empty(awidf_empty)
    ,.rdreq(awidf_rdreq)
);


logic widf_wr_en;
logic widf_full; // if true, ERROR
logic widf_empty;
logic widf_rdreq;

SoftFIFO_0 #(
     .LOG_DEPTH  (FIFO_LOGDEPTH)
) wid_fifo (
     .clock(clk)
    ,.reset_n(~rst)
    ,.wrreq(s_axi_in.wvalid && widf_wr_en)
    ,.full(widf_full)
    ,.empty(widf_empty)
    ,.rdreq(widf_rdreq)
);

logic awidf_valid;
logic awidf_ready;
logic widf_valid;
logic widf_ready;
logic bidf_valid;
logic bidf_ready;

assign bidf_valid = awidf_valid && widf_valid;
assign awidf_ready = bidf_ready && widf_valid;
assign widf_ready = bidf_ready && awidf_valid;

assign awidf_valid = !awidf_empty;
assign widf_valid = !widf_empty;
assign bidf_ready = !bidf_full;

assign bidf_wrreq = bidf_valid;
assign awidf_rdreq = awidf_ready;
assign widf_rdreq = widf_ready;

assign s_axi_out.awready = awidf_wr_en && !awidf_full;
assign s_axi_out.wready = widf_wr_en && !widf_full;
assign s_axi_out.arready = ridf_wr_en && !ridf_full;

localparam BP_MAX1 = 1 << BACKPRESSURE;
localparam BP_MAX = BP_MAX1 - 1;

logic [31:0] counter;

assign awidf_wr_en = (BACKPRESSURE == 0) ? 1'b1 : (counter % BP_MAX1 == BP_MAX);
assign widf_wr_en  = (BACKPRESSURE == 0) ? 1'b1 : (counter % BP_MAX1 == BP_MAX);
assign ridf_wr_en  = (BACKPRESSURE == 0) ? 1'b1 : (counter % BP_MAX1 == 0);

always @(posedge clk) begin
    if (rst) begin
        errors_sticky <= '0;
        counter <= '0;
    end else begin
        if (ridf_full) errors_sticky[0] <= 1;
        if (bidf_full || awidf_full || widf_full) errors_sticky[1] <= 1;
        counter <= counter + 1;
    end
end

endmodule
