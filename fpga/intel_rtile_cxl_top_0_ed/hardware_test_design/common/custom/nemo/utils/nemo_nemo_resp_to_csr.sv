import nemo_defines::*;

module nemo_nemo_resp_to_csr (

    // AVMM Slave Interface
    // input               clk,
    // input               rst,

    output logic        read_valid,
    output logic [63:0] read_data

    ,input  nemo_defines::t_nemo_controlpath_resp_ch                  controlpath_resp
    ,input  nemo_defines::t_nemo_controlpath_resp_valid               controlpath_resp_valid
    ,output nemo_defines::t_nemo_controlpath_resp_ready               controlpath_resp_ready
);

localparam EX_CAP_HEADER = 64'h00000000_00000000;

localparam ERR_BITEXTEND = 64 - (nemo_defines::N_REQ_ERRS + nemo_defines::N_RESP_ERRS);

assign controlpath_resp_ready = 1;
assign read_valid = controlpath_resp_valid && !controlpath_resp.req_metadata.is_write;
always_comb begin
    read_data = controlpath_resp.read_data;

    if (controlpath_resp.req_metadata.req_ok_n[0]) begin
        // Was chain access
        // In ED PF1 capability chain with HEADER E00 terminate here with data zero
        read_data = EX_CAP_HEADER;
    end else if (controlpath_resp.resp_ok_n != 0 || controlpath_resp.req_metadata.req_ok_n != 0) begin
        // generic error
        read_data = { {ERR_BITEXTEND{1'b1}}, controlpath_resp.resp_ok_n, controlpath_resp.req_metadata.req_ok_n};
    end
end

endmodule
