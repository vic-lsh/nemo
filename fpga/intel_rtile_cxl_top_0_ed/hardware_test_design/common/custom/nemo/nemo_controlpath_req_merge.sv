import nemo_defines::*;

module nemo_controlpath_req_merge #(
    parameter CONTROLPATHS_IN = 2
) (
     input  clk
    ,input  rst

    ,input  t_nemo_controlpath_req_ch      [CONTROLPATHS_IN-1:0]  controlpath_reqs_s_in
    ,input  t_nemo_controlpath_req_valid   [CONTROLPATHS_IN-1:0]  controlpath_reqs_s_in_valid
    ,output t_nemo_controlpath_req_ready   [CONTROLPATHS_IN-1:0]  controlpath_reqs_s_out_thenready

    ,output t_nemo_controlpath_req_ch                          controlpath_req_m_out
    ,output t_nemo_controlpath_req_valid                       controlpath_req_m_out_valid
    ,input  t_nemo_controlpath_req_ready                       controlpath_req_m_in_ready
);

generate
    if (CONTROLPATHS_IN != 2) begin
        this_module_doesnt_exist_so_err_ controlpath_reqs_must_be_2_rn_bc_im_lazy();
    end
endgenerate


// tree style merging
module nemo_controlpath_req_2way_merger (
     input  clk
    ,input  rst

    ,input  t_nemo_controlpath_req_ch      [1:0]   stream_i
    ,input  t_nemo_controlpath_req_valid   [1:0]   stream_valid_i
    ,output t_nemo_controlpath_req_ready   [1:0]   stream_thenready_o

    ,output t_nemo_controlpath_req_ch              merged_o
    ,output t_nemo_controlpath_req_valid           merged_valid_o
    ,input  t_nemo_controlpath_req_ready           merged_ready_i
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

nemo_controlpath_req_2way_merger in0_in1_merger (
     .clk                   (clk)
    ,.rst                   (rst)

    ,.stream_i              (controlpath_reqs_s_in[1:0])
    ,.stream_valid_i        (controlpath_reqs_s_in_valid[1:0])
    ,.stream_thenready_o    (controlpath_reqs_s_out_thenready[1:0])

    ,.merged_o              (controlpath_req_m_out)
    ,.merged_valid_o        (controlpath_req_m_out_valid)
    ,.merged_ready_i        (controlpath_req_m_in_ready)
);


function [1:0] [NEMO_CONTROLPATH_REQ_WIDTH-1:0] to_merge_type;
    input t_nemo_controlpath_req_ch[1:0] inps;
    begin
        return {inps[1], inps[0]};
    end
endfunction

endmodule
