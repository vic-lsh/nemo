module nemo_fifo_valrdy #(
     parameter LOG_DEPTH = 2
    ,parameter WIDTH = 64
    ,parameter USE_LUTRAM = 1
) (
     input                  clk
    ,input                  rst
    
    ,input                  wrvalid
    ,input  [WIDTH-1:0]     wrdata
    ,output                 wrready
    
    ,input                  rdready
    ,output [WIDTH-1:0]     rddata
    ,output                 rdvalid
    
    ,output [LOG_DEPTH:0]   approx_length
);

logic full;
assign wrready = ~full;

logic empty;
assign rdvalid = ~empty;

nemo_fifo #(
     .LOG_DEPTH (LOG_DEPTH)
    ,.WIDTH     (WIDTH)
    ,.USE_LUTRAM(USE_LUTRAM)
) nemo_fifo_valrdy_inst (
     .clk           (clk)
    ,.rst           (rst)
    ,.wrreq         (wrvalid)
    ,.wrdata        (wrdata)
    ,.full          (full)
    ,.rdreq         (rdready)
    ,.rddata        (rddata)
    ,.empty         (empty)
    ,.approx_length (approx_length)
);

endmodule
