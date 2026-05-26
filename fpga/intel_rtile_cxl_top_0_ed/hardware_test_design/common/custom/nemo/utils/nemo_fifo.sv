module nemo_fifo #(
     parameter LOG_DEPTH = 2
    ,parameter WIDTH = 64
    ,parameter USE_LUTRAM = 1
) (
     input                  clk
    ,input                  rst
    
    ,input                  wrreq
    ,input  [WIDTH-1:0]     wrdata
    ,output                 full
    
    ,input                  rdreq
    ,output [WIDTH-1:0]     rddata
    ,output                 empty
    
    ,output [LOG_DEPTH:0]   approx_length
);

sync_fifo #(
     .LOG_DEPTH (LOG_DEPTH)
    ,.WIDTH     (WIDTH)
    ,.USE_LUTRAM(USE_LUTRAM)
    ,.USE_OUTREG(1)
    ,.SHOW_AHEAD(1)
    ,.TRUE_VALRDY(1)
) nemo_fifo_inst (
     .clk           (clk)
    ,.rst           (rst)
    ,.wrreq         (wrreq)
    ,.data          (wrdata)
    ,.full          (full)
    ,.q             (rddata)
    ,.empty         (empty)
    ,.rdreq         (rdreq)
    ,.usedw         (approx_length)
    ,.almostempty   ()
    ,.almostfull    ()
    ,.overflow      () // overflow check is on, so should never happen
);

endmodule
