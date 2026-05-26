// tree style merging
module nemo_2way_merger #(
     parameter DTYPE_WIDTH = -1
) (
     input  clk
    ,input  rst

    ,input  [1:0]   [DTYPE_WIDTH-1:0]   stream_i
    ,input  [1:0]                       stream_valid_i
    ,output [1:0]                       stream_thenready_o

    ,output [DTYPE_WIDTH-1:0]   merged_o
    ,output                     merged_valid_o
    ,input                      merged_ready_i
);

generate 
    if (DTYPE_WIDTH == -1) this_module_doesnt_exist_so_err_bc_ you_forgot_dtype_width();
endgenerate

// Priority. alternated on every transaction.
logic prio;

always @(posedge clk) begin
    if (rst) begin
        prio <= 0;
    end else begin
        if (merged_valid_o && merged_ready_i) begin
            prio <= ~prio;
        end
    end
end

logic picked;
/*
prio    valid1  valid0  valido  picked
0       0       0       0       X
0       0       1       1       0
0       1       0       1       1
0       1       1       1       0
1       0       0       0       X
1       0       1       1       0
1       1       0       1       1
1       1       1       1       1
*/

// pick prio if both are valid, otherwise pick the valid one.
assign picked = (stream_valid_i[0] && stream_valid_i[1]) ? prio : stream_valid_i[1];
assign merged_o = stream_i[picked];
assign merged_valid_o = stream_valid_i[0] || stream_valid_i[1];
assign stream_thenready_o[0] = picked == 0 ? merged_ready_i : 0;
assign stream_thenready_o[1] = picked == 1 ? merged_ready_i : 0;

endmodule
