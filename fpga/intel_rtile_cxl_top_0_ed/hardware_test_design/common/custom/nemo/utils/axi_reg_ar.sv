import afu_axi_if_pkg::*;

module axi_reg_ar #(
    parameter LOG_DEPTH = 2,
    parameter EVERY_OTHER = 0
) (
     input clk
    ,input rst

    ,input  t_axi4_rd_addr_ch           s_in_axi_ar
    ,output t_axi4_rd_addr_ready        s_out_axi_arready

    ,output t_axi4_rd_addr_ch           m_out_axi_ar
    ,input  t_axi4_rd_addr_ready        m_in_axi_arready
);

localparam AR_WIDTH = 0
                        + AFU_AXI_MAX_ID_WIDTH
                        + AFU_AXI_MAX_ADDR_WIDTH
                        + AFU_AXI_MAX_BURST_LENGTH_WIDTH
                        + $bits(t_axi4_burst_size_encoding)
                        + $bits(t_axi4_burst_encoding)
                        + $bits(t_axi4_prot_encoding)
                        + $bits(t_axi4_qos_encoding)
                        + $bits(t_axi4_arcache_encoding)
                        + $bits(t_axi4_lock_encoding)
                        + AFU_AXI_REGION_WIDTH
                        + $bits(t_axi4_aruser)
                        + 1;
/*
        logic [AFU_AXI_MAX_ID_WIDTH-1:0]            arid;
        logic [AFU_AXI_MAX_ADDR_WIDTH-1:0]          araddr;
        logic [AFU_AXI_MAX_BURST_LENGTH_WIDTH-1:0]  arlen;
        t_axi4_burst_size_encoding                  arsize;
        t_axi4_burst_encoding                       arburst;
        t_axi4_prot_encoding                        arprot;
        t_axi4_qos_encoding                         arqos;
        logic                                       arvalid;
        t_axi4_arcache_encoding                     arcache;
        t_axi4_lock_encoding                        arlock;
        logic [AFU_AXI_REGION_WIDTH-1:0]            arregion;
        t_axi4_aruser                               aruser;
*/

logic [AR_WIDTH-1:0] fifo_input;
logic [AR_WIDTH-1:0] fifo_output;
logic fifo_output_valid;
logic fifo_ar_valid;
logic transaction_last;

assign fifo_input = {s_in_axi_ar.arvalid, s_in_axi_ar.arid, s_in_axi_ar.araddr, s_in_axi_ar.arlen, s_in_axi_ar.arsize, s_in_axi_ar.arburst, s_in_axi_ar.arprot, s_in_axi_ar.arqos, s_in_axi_ar.arcache, s_in_axi_ar.arlock, s_in_axi_ar.arregion, s_in_axi_ar.aruser};
assign {fifo_ar_valid, m_out_axi_ar.arid, m_out_axi_ar.araddr, m_out_axi_ar.arlen, m_out_axi_ar.arsize, m_out_axi_ar.arburst, m_out_axi_ar.arprot, m_out_axi_ar.arqos, m_out_axi_ar.arcache, m_out_axi_ar.arlock, m_out_axi_ar.arregion, m_out_axi_ar.aruser} = fifo_output;
assign m_out_axi_ar.arvalid = !transaction_last && fifo_output_valid && fifo_ar_valid;

/*
// wastes one bit of space storing the valid bit for convenience.
nemo_fifo_valrdy #(
     .LOG_DEPTH  (LOG_DEPTH)
    ,.WIDTH     (AR_WIDTH)
    ,.USE_LUTRAM(1)
) axi_reg_ar_fifo (
     .clk(clk)
    ,.rst(rst)
    
    ,.wrvalid(s_in_axi_ar.arvalid)
    ,.wrdata(fifo_input)
    ,.wrready(s_out_axi_arready)
    
    ,.rdready(m_in_axi_arready)
    ,.rddata(fifo_output)
    ,.rdvalid(fifo_output_valid)
    
    ,.approx_length()
);*/
logic full;
logic empty;
assign s_out_axi_arready = !full;
assign fifo_output_valid = !empty;
wire rdreq = (!transaction_last && m_in_axi_arready) || (fifo_output_valid && !fifo_ar_valid);
SoftFIFO #(
     .LOG_DEPTH  (LOG_DEPTH)
    ,.WIDTH     (AR_WIDTH)
) axi_reg_ar_fifo_soft (
     .clock(clk)
    ,.reset_n(~rst)
    ,.wrreq(s_in_axi_ar.arvalid)
    ,.data(fifo_input)
    ,.full(full)
    ,.q(fifo_output)
    ,.empty(empty)
    ,.rdreq(rdreq)
);
always_ff @(posedge clk) begin
    if (rst) transaction_last <= (EVERY_OTHER ? 1 : 0);
    else begin
        if (EVERY_OTHER == 1) begin
            transaction_last <= m_out_axi_ar.arvalid && m_in_axi_arready;
        end else if (EVERY_OTHER == 2) begin
            transaction_last <= !transaction_last;
        end else begin
            transaction_last <= 0;
        end
    end
end
/*
things to try:
* two ar softfifos (maybe displacing signals by 1 is bad?? or do 4 to make clocks certainly sync??)
* custom axi slave (maybe we're getting a bus error)
* queue, print all ARs thru afu_top
* queue, print all Rs thru afu_top (mb not data)
* sign extension for address instead of 0 extend (but why would this matter?) NVM this is dumb don't do, we truncate anyways lol
* ar passthru but addr + 64 or something
* consider bus deadlock
* consider if i can just have a flipflop instead of 2 transactions
* DO +64 BUT with only one valid page. I think +64 is really doing like *3 lol. and that is fing us. wtf lol.
* maybe delaying ar but not aw is bad? but the cpu shouldn't depend on the timing of each channel...
* maybe we need to use full/empty then wr/rdreq? but it worked before...
* go back to OG: everything had 2 fifos + 1 more for ar.
* explicitly remove the valid=0 things from the fifo without displaying it. didn't work.
* go back to prev design with new modules
* slowly start copying old design back in
* ar reg before nemo type converter work?
* try to detect if host is breaking axi spec. i think it is.
* just ar. nothing else. empty afu_top. and just axi_reg too.
* with just one fifo, append output of nemo_top to arlog (on "channel 1")

* assume ready is respected one cycle late, so send it one cycle later (fifo is almost full instead of full)
  * test with huge axi regs
  * test with dummy slave
* recreate CXL IP with correct device number, slower clock
* they have a simulate the IP core section in the docs!
* ensure latest IP & quartus version (qartus 24.3.1 , ip 1.15)

oldnew00 - broken
oldnew01 - broken
oldnew11 - works
oldnew10 - broken
possible conclusion: arfifo is broken and also control needs to be driven. building oldnew200 and oldnew201

delaywrites - broken
consumeinvalids - broken
demux-nodebugq - broken ??????
fullfix15noar - works
twofifos - works!!!!!!!!!!

* try axi firewall, more combos of backpressure + axi_reg
* upgrade quartus & cxlip & mcip (vic)
* get new_design working with csr controlpath
* document attempted efforts & results
* try old designs
* backpressure bc 2 channels?
* what about if the mcip can't accept every cycle? invalidate for 1 after transaction (e.g. add delay after arreg?)

* bus error 0x18000c00 => write responses are invalid. does that also lock up read channels when a fault occurs? bc i would have thought that happens too on rd chan.
  * can we clear the fault?

*/

generate;
    //if (AR_WIDTH + 1 != AFU_AXI_RD_ADDR_CH_WIDTH) err_me_pls you_probs_got_axi_reg_ar_wrong();
    if (AR_WIDTH != AFU_AXI_RD_ADDR_CH_WIDTH) err_me_pls you_probs_got_axi_reg_ar_wrong();
endgenerate

endmodule
