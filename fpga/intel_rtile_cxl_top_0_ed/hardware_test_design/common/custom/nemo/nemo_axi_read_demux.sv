import afu_axi_if_pkg::*;

module nemo_axi_read_demux #(
  parameter SIM = 1,
  parameter NERRORS = 9
) (
     input  logic                           clk
    ,input  logic                           rst

    // Slave Port
    ,input  t_axi4_rd_addr_ch               s_in_axi_ar
    ,output t_axi4_rd_addr_ready            s_out_axi_arthenready
    ,output t_axi4_rd_resp_ch               s_out_axi_r
    ,input  t_axi4_rd_resp_ready            s_in_axi_rready

    // Master Ports
    ,output t_axi4_rd_addr_ch       [1:0]   m_out_axi_ar
    ,input  t_axi4_rd_addr_ready    [1:0]   m_in_axi_arready
    ,input  t_axi4_rd_resp_ch       [1:0]   m_in_axi_r
    ,output t_axi4_rd_resp_ready    [1:0]   m_out_axi_rready
  
    ,input                                  slv_ar_select_i // ASSUMPTION: once valid is asserted, this doesn't change throughout the axi ar handshake.
    ,output [NERRORS-1:0]                   errors_out

);

logic [NERRORS-1:0] errors;
logic [NERRORS-1:0] errors_next;
assign errors_out = errors;
always_ff @(posedge clk) begin
    if (rst) begin
        errors <= '0;
    end else begin
        for (integer i = 0; i < NERRORS; i++) begin
            if (errors_next[i]) begin
                errors[i] <= 1;
                $fatal(1, "error in axi demux simpler. error %d at %d", errors_next, i);
            end
        end
    end
end

/*
assertions:

(0) slv_ar_select_i behaves, doesn't change once valid is set until ready is set.
(1,2,3,4,5,6) once held, every valid is held until ready.
(7) fifo shouldn't be empty ever when a read comes back from either device
(8)
*/
// assertion 0
logic a0_clocked_select;
logic a0_clocked_select_stored;
always @(posedge clk) begin
    if (rst) begin
        a0_clocked_select_stored <= 0;
        a0_clocked_select <= 'X;
        errors_next[0] <= 0;
    end else begin
        if (s_in_axi_ar.arvalid && s_out_axi_arthenready) begin
            a0_clocked_select_stored <= 0;
            a0_clocked_select <= 'X;
        end else if (s_in_axi_ar.arvalid && ~a0_clocked_select_stored) begin
            a0_clocked_select_stored <= 1;
            a0_clocked_select <= slv_ar_select_i;
        end else if (a0_clocked_select_stored) begin
            if (a0_clocked_select != slv_ar_select_i) begin
                errors_next[0] <= 1;
            end
        end
    end
end
// assertions 1,2,3,4,5,6
logic [5:0]valid_last_cycle;
logic [5:0]valid_this_cycle;
logic [5:0]ready_last_cycle;
assign valid_this_cycle[0] = s_in_axi_ar.arvalid;
assign valid_this_cycle[1] = m_out_axi_ar[0].arvalid;
assign valid_this_cycle[2] = m_out_axi_ar[1].arvalid;
assign valid_this_cycle[3] = s_out_axi_r.rvalid;
assign valid_this_cycle[4] = m_in_axi_r[0].rvalid;
assign valid_this_cycle[5] = m_in_axi_r[1].rvalid;
always @(posedge clk) begin
    if (rst) begin
        valid_last_cycle <= '0;
        ready_last_cycle <= '0;
        errors_next[1] <= 0;
        errors_next[2] <= 0;
        errors_next[3] <= 0;
        errors_next[4] <= 0;
        errors_next[5] <= 0;
        errors_next[6] <= 0;
    end else begin
        ready_last_cycle[0] <= s_out_axi_arthenready;
        ready_last_cycle[1] <= m_in_axi_arready[0];
        ready_last_cycle[2] <= m_in_axi_arready[1];
        ready_last_cycle[3] <= s_in_axi_rready;
        ready_last_cycle[4] <= m_out_axi_rready[0];
        ready_last_cycle[5] <= m_out_axi_rready[1];
        for (integer ii = 0; ii < 6; ii++) begin
            if (valid_last_cycle[ii] && ~ready_last_cycle[ii] && ~valid_this_cycle[ii]) begin
                errors_next[1+ii] <= 1;
            end
            valid_last_cycle[ii] <= valid_this_cycle[ii];
        end
    end
end

// AR PATH

logic ordering_fifo_wr_val;
logic ordering_fifo_wr_data;
logic ordering_fifo_wr_full;
wire ordering_fifo_wr_rdy = ~ordering_fifo_wr_full;
wire ordering_fifo_wr_happened = ordering_fifo_wr_val && ordering_fifo_wr_rdy;
logic [1:0] ordering_fifo_wr_happened_on;
assign ordering_fifo_wr_happened_on[0] = ordering_fifo_wr_happened && ordering_fifo_wr_data == 0;
assign ordering_fifo_wr_happened_on[1] = ordering_fifo_wr_happened && ordering_fifo_wr_data == 1;

// we can accept a new ar as long as the real slave device can accept it and the fifo is ready for a write
assign s_out_axi_arthenready = ordering_fifo_wr_rdy && m_in_axi_arready[slv_ar_select_i];
assign ordering_fifo_wr_val = s_in_axi_ar.arvalid && m_in_axi_arready[slv_ar_select_i]; // basically if s_in_axi_ar.arvalid && s_out_axi_arthenready, but simplified to take out the dependence on the fifo rdy
assign ordering_fifo_wr_data = (SIM && ~ordering_fifo_wr_happened) ? 'X : slv_ar_select_i;
generate
for (genvar i = 0; i < 2; i = i + 1) begin: assign_ar_outputs
    assign m_out_axi_ar[i].arvalid = ordering_fifo_wr_rdy && s_in_axi_ar.arvalid && slv_ar_select_i == i;

    assign m_out_axi_ar[i].arid     = (SIM && ~ordering_fifo_wr_happened_on[i]) ? 'X : s_in_axi_ar.arid;
    assign m_out_axi_ar[i].araddr   = (SIM && ~ordering_fifo_wr_happened_on[i]) ? 'X : s_in_axi_ar.araddr;
    assign m_out_axi_ar[i].arlen    = (SIM && ~ordering_fifo_wr_happened_on[i]) ? 'X : s_in_axi_ar.arlen;
    assign m_out_axi_ar[i].arsize   = (SIM && ~ordering_fifo_wr_happened_on[i]) ? t_axi4_burst_size_encoding'('X) : s_in_axi_ar.arsize;
    assign m_out_axi_ar[i].arburst  = (SIM && ~ordering_fifo_wr_happened_on[i]) ? t_axi4_burst_encoding'(     'X) : s_in_axi_ar.arburst;
    assign m_out_axi_ar[i].arprot   = (SIM && ~ordering_fifo_wr_happened_on[i]) ? t_axi4_prot_encoding'(      'X) : s_in_axi_ar.arprot;
    assign m_out_axi_ar[i].arqos    = (SIM && ~ordering_fifo_wr_happened_on[i]) ? t_axi4_qos_encoding'(       'X) : s_in_axi_ar.arqos;
    assign m_out_axi_ar[i].arcache  = (SIM && ~ordering_fifo_wr_happened_on[i]) ? t_axi4_arcache_encoding'(   'X) : s_in_axi_ar.arcache;
    assign m_out_axi_ar[i].arlock   = (SIM && ~ordering_fifo_wr_happened_on[i]) ? t_axi4_lock_encoding'(      'X) : s_in_axi_ar.arlock;
    assign m_out_axi_ar[i].arregion = (SIM && ~ordering_fifo_wr_happened_on[i]) ? 'X : s_in_axi_ar.arregion;
    assign m_out_axi_ar[i].aruser   = (SIM && ~ordering_fifo_wr_happened_on[i]) ? t_axi4_aruser'(             'X) : s_in_axi_ar.aruser;
end
endgenerate

// R PATH

logic ordering_fifo_rd_rdy;
logic ordering_fifo_rd_data;
logic ordering_fifo_rd_empty;
wire ordering_fifo_rd_val = ~ordering_fifo_rd_empty;
wire ordering_fifo_rd_happened = ordering_fifo_rd_val && ordering_fifo_rd_rdy;

assign s_out_axi_r.rvalid = ordering_fifo_rd_val && m_in_axi_r[ordering_fifo_rd_data].rvalid;
assign ordering_fifo_rd_rdy = m_in_axi_r[ordering_fifo_rd_data].rvalid && s_in_axi_rready && m_in_axi_r[ordering_fifo_rd_data].rlast;

assign s_out_axi_r.rid      = (SIM && ~ordering_fifo_rd_val) ? 'X : m_in_axi_r[ordering_fifo_rd_data].rid;
assign s_out_axi_r.rdata    = (SIM && ~ordering_fifo_rd_val) ? 'X : m_in_axi_r[ordering_fifo_rd_data].rdata;
assign s_out_axi_r.rresp    = (SIM && ~ordering_fifo_rd_val) ? t_axi4_resp_encoding'('X) : m_in_axi_r[ordering_fifo_rd_data].rresp;
assign s_out_axi_r.rlast    = (SIM && ~ordering_fifo_rd_val) ? 'X : m_in_axi_r[ordering_fifo_rd_data].rlast;
assign s_out_axi_r.ruser    = (SIM && ~ordering_fifo_rd_val) ? t_axi4_ruser'('X) : m_in_axi_r[ordering_fifo_rd_data].ruser;


generate
for (genvar j = 0; j < 2; j = j + 1) begin: assign_r_outputs
    assign m_out_axi_rready[j] = ordering_fifo_rd_val && s_in_axi_rready && ordering_fifo_rd_data == j;
end
endgenerate

// this fifo spec is dumb and doesn't check for overflow or underflow...
wire ordering_fifo_wr_wrreq = ordering_fifo_wr_val && ~ordering_fifo_wr_full;
wire ordering_fifo_rd_rdreq = ordering_fifo_rd_rdy && ~ordering_fifo_rd_empty;

sync_fifo #(
    .LOG_DEPTH (10),
    .WIDTH     (1),
    .USE_LUTRAM(0),
    .USE_OUTREG(1),
    .SHOW_AHEAD(1)
) rd_req_ordering_fifo (
    .rst        (rst),
    .clk        (clk),
    .wrreq      (ordering_fifo_wr_wrreq),
    .data       (ordering_fifo_wr_data),
    .rdreq      (ordering_fifo_rd_rdreq),
    .q          (ordering_fifo_rd_data),
    .full       (ordering_fifo_wr_full),
    .almostfull (),
    .empty      (ordering_fifo_rd_empty),
    .almostempty(),
    .overflow   (),
    .usedw      ()
);

// assertion 7
// it shouldn't be possible for read data to exist without a route back.
localparam ELIMIT = 5;
logic [$clog2(ELIMIT):0]empty_in_a_row;
always @(posedge clk) begin
    if (rst) begin
	empty_in_a_row <= '0;
        errors_next[7] <= 0;
    end else if (ordering_fifo_rd_empty && (m_in_axi_r[0].rvalid || m_in_axi_r[1].rvalid)) begin
	empty_in_a_row <= empty_in_a_row + 1;
	if (empty_in_a_row == ELIMIT) begin
            errors_next[7] <= 1;
	end
    end else begin
	empty_in_a_row <= '0;
    end
end

// assertion 8
// given that we don't register the AR reqs, it shouldn't be possible for the FIFO to fill up, or else we've imposed limitations on the mem BW
always @(posedge clk) begin
    if (rst) begin
        errors_next[8] <= 0;
    end else if (ordering_fifo_wr_full) begin
        errors_next[8] <= 1;
    end
end
endmodule
