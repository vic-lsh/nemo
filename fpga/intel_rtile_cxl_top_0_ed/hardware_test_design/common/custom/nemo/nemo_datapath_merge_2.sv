import nemo_defines::*;

// tree style merging
module nemo_datapath_merge_2 (
     input  clk
    ,input  rst

    ,input  t_nemo_datapath_ch      [1:0]   stream_i
    ,input  t_nemo_datapath_valid   [1:0]   stream_valid_i
    ,output t_nemo_datapath_ready   [1:0]   stream_thenready_o

    ,output t_nemo_datapath_ch              merged_o
    ,output t_nemo_datapath_valid           merged_valid_o
    ,input  t_nemo_datapath_ready           merged_ready_i
);

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